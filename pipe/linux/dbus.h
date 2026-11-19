#ifndef VANILLA_DBUS_H
#define VANILLA_DBUS_H

#include <gio/gio.h>

typedef struct {
    char *owner;
    char *device_path;
    char *connection_path;
    gboolean restore_managed;
} vanilla_nm_state_t;

int vanilla_nm_acquire(GDBusConnection *bus, const char *interface, vanilla_nm_state_t *state);
void vanilla_nm_release(GDBusConnection *bus, const char *interface, vanilla_nm_state_t *state);
int vanilla_iwd_disconnect(GDBusConnection *bus, const char *interface);

#endif
