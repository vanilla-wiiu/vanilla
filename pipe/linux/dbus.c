#include "dbus.h"
#include "wpa.h"

#include <string.h>

#define NM "org.freedesktop.NetworkManager"
#define NM_PATH "/org/freedesktop/NetworkManager"
#define NM_DEVICE NM ".Device"
#define NM_ACTIVE NM ".Connection.Active"
#define IWD "net.connman.iwd"
#define IWD_DEVICE IWD ".Device"
#define IWD_STATION IWD ".Station"
#define TIMEOUT_MS 10000

static GVariant *call(GDBusConnection *bus,
                      const char *owner,
                      const char *path,
                      const char *interface,
                      const char *method,
                      GVariant *args,
                      const char *reply_type,
                      GError **error)
{
    // Make synchronous call to dbus
    return g_dbus_connection_call_sync(
        bus,
        owner,
        path,
        interface,
        method,
        args,
        G_VARIANT_TYPE(reply_type),
        G_DBUS_CALL_FLAGS_NO_AUTO_START,
        TIMEOUT_MS,
        NULL,
        error
    );
}

static int failed(const char *operation, GError *error)
{
    // Print dbus error
    nlprint("%s failed: %s", operation, error ? error->message : "invalid D-Bus reply");
    return -1;
}

static char *get_owner(GDBusConnection *bus, const char *name, GError **error)
{
    // Get name of connection currently "owning" the bus we want
    g_autoptr(GVariant) reply = call(
        bus,
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "GetNameOwner",
        g_variant_new("(s)", name),
        "(s)",
        error
    );
    if (!reply) {
        if (g_error_matches(*error, G_DBUS_ERROR, G_DBUS_ERROR_NAME_HAS_NO_OWNER)) {
            g_clear_error(error);
        }
        return NULL;
    }

    char *owner;
    g_variant_get(reply, "(s)", &owner);
    return owner;
}

static GVariant *properties(GDBusConnection *bus, const char *owner, const char *path,
                            const char *interface, GError **error)
{
    // Retrieve all properties for this interface on this object
    g_autoptr(GVariant) reply = call(
        bus,
        owner,
        path,
        "org.freedesktop.DBus.Properties",
        "GetAll",
        g_variant_new("(s)", interface),
        "(a{sv})",
        error
    );
    return reply ? g_variant_get_child_value(reply, 0) : NULL;
}

static int set_managed(GDBusConnection *bus, const char *interface,
                       vanilla_nm_state_t *state, gboolean managed)
{
    g_autoptr(GError) error = NULL;

    // Try the (relatively) new SetManaged call (in NetworkManager 1.56+) with
    // flag NM_DEVICE_MANAGED_FLAGS_RUNTIME (0x1), so the change is temporary
    // https://networkmanager.dev/docs/api/1.56/nm-dbus-types.html#NMDeviceManagedFlags
    g_autoptr(GVariant) reply = call(
        bus,
        state->owner,
        state->device_path,
        NM_DEVICE,
        "SetManaged",
        g_variant_new("(uu)", managed ? 1 : 0, 0x1),
        "()",
        &error
    );

    // If we're on an older NetworkManager version without that API, try the
    // older method
    if (!reply && g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD)) {
        g_clear_error(&error);
        reply = call(
            bus,
            state->owner,
            state->device_path,
            "org.freedesktop.DBus.Properties",
            "Set",
            g_variant_new("(ssv)", NM_DEVICE, "Managed", g_variant_new_boolean(managed)),
            "()",
            &error
        );
    }
    if (!reply) {
        return failed("NETWORKMANAGER: setting Managed", error);
    }

    if (managed) {
        // We're setting it back to managed, we don't really care or need to wait
        // for it to actually happen
        return 0;
    }

    // We're setting to unmanaged so we can use the interface ourselves. Poll
    // until NetworkManager confirms the device is indeed unmanaged.
    gint64 deadline = g_get_monotonic_time() + TIMEOUT_MS * G_TIME_SPAN_MILLISECOND;
    while (1) {
        g_autoptr(GVariant) props = properties(bus, state->owner, state->device_path, NM_DEVICE, &error);
        if (!props) {
            return failed("NETWORKMANAGER: reading device state", error);
        }

        gboolean current_managed;
        guint32 device_state;
        if (!g_variant_lookup(props, "Managed", "b", &current_managed)
            || !g_variant_lookup(props, "State", "u", &device_state)) {
            return failed("NETWORKMANAGER: reading device state", NULL);
        }

        if (!current_managed && device_state == 10 /* NM_DEVICE_STATE_UNMANAGED */) {
            nlprint("NETWORKMANAGER: %s is unmanaged; deactivation complete", interface);
            return 0;
        }

        if (g_get_monotonic_time() >= deadline) {
            nlprint("NETWORKMANAGER: timed out waiting for %s to become unmanaged", interface);
            return -1;
        }

        g_usleep(50000);
    }
}

int vanilla_nm_acquire(GDBusConnection *bus, const char *interface, vanilla_nm_state_t *state)
{
    // Locate connection "owning" NetworkManager bus we want
    g_autoptr(GError) error = NULL;
    state->owner = get_owner(bus, NM, &error);
    if (!state->owner) {
        if (error) {
            return failed("NETWORKMANAGER: finding daemon", error);
        }

        nlprint("NETWORKMANAGER: not running; no management handoff needed");
        return 0;
    }

    // List interfaces
    g_autoptr(GVariant) reply = call(bus, state->owner, NM_PATH, NM, "GetDevices", NULL, "(ao)", &error);
    if (!reply) {
        return failed("NETWORKMANAGER: enumerating devices", error);
    }

    // Loop through devices to see if we can find the requested interface and
    // whether it is currently managed or not
    g_autoptr(GVariant) devices = g_variant_get_child_value(reply, 0);
    GVariantIter iter;
    const char *path;
    g_variant_iter_init(&iter, devices);
    while (g_variant_iter_next(&iter, "&o", &path)) {
        // Get all properties of device
        g_autoptr(GVariant) props = properties(bus, state->owner, path, NM_DEVICE, &error);
        if (!props) {
            if (g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_OBJECT)) {
                g_clear_error(&error); // Hot-unplugged since GetDevices.
                continue;
            }
            return failed("NETWORKMANAGER: reading device", error);
        }

        // Determine if it is the interface we're looking for
        const char *name, *active_path;
        if (!g_variant_lookup(props, "Interface", "&s", &name) || strcmp(name, interface)) {
            continue;
        }

        // If so, store its current "managed" state in our state
        if (!g_variant_lookup(props, "Managed", "b", &state->restore_managed)) {
            return failed("NETWORKMANAGER: reading Managed", NULL);
        }

        state->device_path = g_strdup(path);

        // Determine if it is an active connection right now so we can restore it
        // once we're closed (especially important for setups with iwd which we
        // manipulate directly)
        if (state->restore_managed
            && g_variant_lookup(props, "ActiveConnection", "&o", &active_path)
            && strcmp(active_path, "/")) {
            g_autoptr(GVariant) active = properties(bus, state->owner, active_path, NM_ACTIVE, &error);
            const char *profile;
            if (!active || !g_variant_lookup(active, "Connection", "&o", &profile)) {
                return failed("NETWORKMANAGER: saving previous connection", error);
            }
            if (strcmp(profile, "/")) {
                state->connection_path = g_strdup(profile);
            }
        }

        // Set intended managed state
        return set_managed(bus, interface, state, FALSE) == 0 ? 1 : -1;
    }

    // Couldn't find device in NetworkManager. Not fatal, could just be a platform
    // that doesn't use NetworkManager (e.g. our Buildroot distros)
    nlprint("NETWORKMANAGER: does not manage interface %s", interface);
    return 0;
}

void vanilla_nm_release(GDBusConnection *bus, const char *interface, vanilla_nm_state_t *state)
{
    // Restore managed state and previous connection
    if (state->restore_managed && state->device_path && set_managed(bus, interface, state, TRUE) == 0) {
        nlprint("NETWORKMANAGER: restored management of %s", interface);

        // Explicitly restore the connection because we disabled iwd autoconnect
        if (state->connection_path) {
            g_autoptr(GError) error = NULL;
            g_autoptr(GVariant) reply = call(
                bus,
                state->owner,
                NM_PATH,
                NM,
                "ActivateConnection",
                g_variant_new("(ooo)", state->connection_path, state->device_path, "/"),
                "(o)",
                &error
            );

            if (!reply) {
                failed("NETWORKMANAGER: restoring previous connection", error);
            } else {
                nlprint("NETWORKMANAGER: requested restoration of previous connection on %s", interface);
            }
        }
    }

    g_clear_pointer(&state->owner, g_free);
    g_clear_pointer(&state->device_path, g_free);
    g_clear_pointer(&state->connection_path, g_free);
    state->restore_managed = FALSE;
}

static gboolean iwd_error_is(GError *error, const char *name)
{
    g_autofree char *remote = g_dbus_error_get_remote_error(error);
    return g_strcmp0(remote, name) == 0;
}

int vanilla_iwd_disconnect(GDBusConnection *bus, const char *interface)
{
    // Send "Disconnect" message to iwd, which stops the current connection
    // and disables autoconnecting to any other on this interface
    // https://kernel.googlesource.com/pub/scm/network/wireless/iwd/+/master/doc/station-api.txt

    // Get owner currently managing iwd connection
    g_autoptr(GError) error = NULL;
    g_autofree char *owner = get_owner(bus, IWD, &error);
    if (!owner) {
        if (error) {
            return failed("IWD: finding daemon", error);
        }

        nlprint("IWD: not running");
        return 0;
    }

    // Get objects (devices) currently being managed by iwd
    g_autoptr(GVariant) reply = call(
        bus,
        owner,
        "/",
        "org.freedesktop.DBus.ObjectManager",
        "GetManagedObjects",
        NULL,
        "(a{oa{sa{sv}}})",
        &error
    );
    if (!reply) {
        return failed("IWD: enumerating devices", error);
    }

    // Find our interface in the device list
    g_autoptr(GVariant) objects = g_variant_get_child_value(reply, 0);
    GVariantIter iter;
    const char *path;
    GVariant *interfaces;
    g_autofree char *station_path = NULL;
    g_variant_iter_init(&iter, objects);
    while (g_variant_iter_next(&iter, "{&o@a{sa{sv}}}", &path, &interfaces)) {
        // Determine if interface name matches the one we're looking for
        g_autoptr(GVariant) current = interfaces;
        g_autoptr(GVariant) device = g_variant_lookup_value(current, IWD_DEVICE, G_VARIANT_TYPE("a{sv}"));
        const char *name;
        if (!device || !g_variant_lookup(device, "Name", "&s", &name) || strcmp(name, interface)) {
            continue;
        }

        // Get path of interface
        g_autoptr(GVariant) station = g_variant_lookup_value(current, IWD_STATION, G_VARIANT_TYPE("a{sv}"));
        if (!station) {
            return failed("IWD: finding Station interface", NULL);
        }

        station_path = g_strdup(path);
        break;
    }

    if (!station_path) {
        nlprint("IWD: does not manage %s; no autoconnect handoff needed", interface);
        return 0;
    }

    // Send "Disconnect" signal to stop any current connection AND prevent iwd
    // from trying to autoconnect to anything else
    nlprint("IWD: requesting disconnect and disabling autoconnect on %s (%s)", interface, station_path);
    gint64 deadline = g_get_monotonic_time() + TIMEOUT_MS * G_TIME_SPAN_MILLISECOND;
    while (1) {
        g_autoptr(GVariant) disconnected = call(bus, owner, station_path, IWD_STATION, "Disconnect", NULL, "()", &error);
        if (disconnected) {
            // Disconnected successfully
            break;
        }

        if (iwd_error_is(error, IWD ".NotConnected")) {
            // Already not connected, nothing to  be done
            g_clear_error(&error);
            break;
        }

        // It's "busy", so we'll wait for it up to "deadline"
        if (!iwd_error_is(error, IWD ".Busy") || g_get_monotonic_time() >= deadline) {
            // Interface remained perpetually busy, so we'll stop trying
            return failed("IWD: disconnecting station", error);
        }

        g_clear_error(&error);
        g_usleep(50000);
    }

    // Ensure "Disconnect" state is what we expect
    while (1) {
        const char *state;
        gboolean scanning;

        // Get interface properties
        g_autoptr(GVariant) props = properties(bus, owner, station_path, IWD_STATION, &error);
        if (!props) {
            return failed("IWD: reading station state", error);
        }

        // Look for "state" in properies
        if (!g_variant_lookup(props, "State", "&s", &state) || !g_variant_lookup(props, "Scanning", "b", &scanning)) {
            return failed("IWD: reading station state", NULL);
        }

        // Ensure it says it's disconnected from iwd. If so, return success.
        if (!strcmp(state, "disconnected") && !scanning) {
            nlprint("IWD: %s disconnected, autoconnect disabled, scanning stopped; leaving iwd running", interface);
            return 1;
        }

        // Otherwise, wait up to "deadline" for it to disconnect
        if (g_get_monotonic_time() >= deadline) {
            nlprint("IWD: timed out waiting for %s to become idle (state=%s, scanning=%s)", interface, state, scanning ? "true" : "false");
            return -1;
        }
        g_usleep(50000);
    }
}
