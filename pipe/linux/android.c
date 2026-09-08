#include "android.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <linux/fib_rules.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <limits.h>
#include <net/if.h>
#include <netlink/addr.h>
#include <netlink/netlink.h>
#include <netlink/route/nexthop.h>
#include <netlink/route/route.h>
#include <netlink/route/rule.h>
#include <netlink/socket.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "wpa.h"

static int restore_wifi;
static int restore_wifi_scan_always;

#define ANDROID_WIFI_SHUTDOWN_TIMEOUT_MS 30000
#define ANDROID_WIFI_QUIET_PERIOD_MS 6000
#define ANDROID_WIFI_POLL_INTERVAL_US 250000
#define ANDROID_WIFI_STATUS_POLL_INTERVAL_MS 1000

#define VANILLA_CONSOLE_ADDRESS "192.168.1.10/32"
#define VANILLA_CONSOLE_RULE_PRIORITY 9999

static struct rtnl_route *console_route;
static struct rtnl_rule *console_rule;

typedef enum {
    ANDROID_WIFI_STATE_UNKNOWN,
    ANDROID_WIFI_STATE_DISABLED,
    ANDROID_WIFI_STATE_NOT_DISABLED,
} android_wifi_state_t;

typedef enum {
    ANDROID_WIFI_STATUS_UNAVAILABLE,
    ANDROID_WIFI_STATUS_CMD,
    ANDROID_WIFI_STATUS_DUMPSYS,
} android_wifi_status_source_t;

static struct rtnl_route *create_console_route(int ifindex,
                                               struct nl_addr *destination,
                                               struct nl_addr *source_address)
{
    struct rtnl_route *route = rtnl_route_alloc();
    struct rtnl_nexthop *next_hop = rtnl_route_nh_alloc();
    if (!route || !next_hop) {
        if (route)
            rtnl_route_put(route);
        if (next_hop)
            rtnl_route_nh_free(next_hop);
        return NULL;
    }

    rtnl_route_set_family(route, AF_INET);
    rtnl_route_set_table(route, RT_TABLE_MAIN);
    rtnl_route_set_protocol(route, RTPROT_STATIC);
    rtnl_route_set_scope(route, RT_SCOPE_LINK);
    rtnl_route_set_type(route, RTN_UNICAST);
    if (rtnl_route_set_dst(route, destination) < 0 ||
        rtnl_route_set_pref_src(route, source_address) < 0) {
        rtnl_route_put(route);
        rtnl_route_nh_free(next_hop);
        return NULL;
    }

    rtnl_route_nh_set_ifindex(next_hop, ifindex);
    rtnl_route_add_nexthop(route, next_hop);
    return route;
}

static struct rtnl_rule *create_console_rule(struct nl_addr *destination)
{
    struct rtnl_rule *rule = rtnl_rule_alloc();
    if (rule) {
        rtnl_rule_set_family(rule, AF_INET);
        rtnl_rule_set_prio(rule, VANILLA_CONSOLE_RULE_PRIORITY);
        rtnl_rule_set_table(rule, RT_TABLE_MAIN);
        rtnl_rule_set_action(rule, FR_ACT_TO_TBL);
        rtnl_rule_set_dst(rule, destination);
    }
    return rule;
}

int vanilla_android_install_console_routing(struct nl_sock *socket,
                                            int ifindex,
                                            struct nl_addr *source_address)
{
    // Remove any previously made routing (during this session)
    vanilla_android_remove_console_routing(socket);

    // Sanity check input
    struct nl_addr *destination = NULL;
    if (!socket || ifindex <= 0 || !source_address || nl_addr_parse(VANILLA_CONSOLE_ADDRESS, AF_INET, &destination) < 0) {
        nlprint("ANDROID: INVALID INPUT");
        goto die;
    }

    // Build route and rule
    struct rtnl_route *route = create_console_route(ifindex, destination, source_address);
    struct rtnl_rule *rule = create_console_rule(destination);
    nl_addr_put(destination);
    if (!route || !rule) {
        nlprint("ANDROID: FAILED TO BUILD WII U ROUTING");
        if (route) {
            rtnl_route_put(route);
        }
        if (rule) {
            rtnl_rule_put(rule);
        }
        goto die;
    }

    // Remove an existing rule if a previous pipe was killed without cleaning up
    rtnl_rule_delete(socket, rule, 0);

    // Add route
    int result = rtnl_route_add(socket, route, NLM_F_CREATE | NLM_F_REPLACE);
    if (result < 0) {
        nlprint("ANDROID: FAILED TO ADD WII U ROUTE: %s", nl_geterror(result));
        goto die_and_put;
    }

    // Add rule
    result = rtnl_rule_add(socket, rule, NLM_F_CREATE | NLM_F_EXCL);
    if (result < 0) {
        nlprint("ANDROID: FAILED TO ADD WII U ROUTING RULE: %s", nl_geterror(result));
        goto die_and_delete_route;
    }

    // Everything worked, store route/rule so we can remove later
    console_route = route;
    console_rule = rule;
    nlprint("ANDROID: NETWORK ROUTE AND RULE ADDED");
    return 0;

die_and_delete_route:
    rtnl_route_delete(socket, route, 0);

die_and_put:
    rtnl_route_put(route);
    rtnl_rule_put(rule);

die:
    return -1;
}

void vanilla_android_remove_console_routing(struct nl_sock *socket)
{
    if (!socket) {
        return;
    }

    if (console_rule) {
        rtnl_rule_delete(socket, console_rule, 0);
        rtnl_rule_put(console_rule);
        console_rule = NULL;
    }

    if (console_route) {
        rtnl_route_delete(socket, console_route, 0);
        rtnl_route_put(console_route);
        console_route = NULL;
    }
}

static android_wifi_state_t parse_wifi_state(const char *status)
{
    if (strstr(status, "Wifi is disabled") ||
        strstr(status, "Wi-Fi is disabled"))
        return ANDROID_WIFI_STATE_DISABLED;

    if (strstr(status, "Wifi is enabled") ||
        strstr(status, "Wi-Fi is enabled") ||
        strstr(status, "Wifi is enabling") ||
        strstr(status, "Wi-Fi is enabling") ||
        strstr(status, "Wifi is disabling") ||
        strstr(status, "Wi-Fi is disabling"))
        return ANDROID_WIFI_STATE_NOT_DISABLED;

    return ANDROID_WIFI_STATE_UNKNOWN;
}

static android_wifi_state_t query_wifi_state(android_wifi_status_source_t source)
{
    const char **command;
    const char *cmd_status[] = {"cmd", "wifi", "status", NULL};
    const char *dumpsys_status[] = {"dumpsys", "wifi", NULL};

    if (source == ANDROID_WIFI_STATUS_CMD) {
        command = cmd_status;
    } else if (source == ANDROID_WIFI_STATUS_DUMPSYS) {
        command = dumpsys_status;
    } else {
        return ANDROID_WIFI_STATE_UNKNOWN;
    }

    char status[256];
    ssize_t status_len = run_process_and_read_stdout(
        command, status, sizeof(status) - 1);
    if (status_len <= 0)
        return ANDROID_WIFI_STATE_UNKNOWN;

    status[status_len] = 0;
    return parse_wifi_state(status);
}

static android_wifi_status_source_t find_wifi_status_source(android_wifi_state_t *state)
{
    *state = query_wifi_state(ANDROID_WIFI_STATUS_CMD);
    if (*state != ANDROID_WIFI_STATE_UNKNOWN) {
        return ANDROID_WIFI_STATUS_CMD;
    }

    *state = query_wifi_state(ANDROID_WIFI_STATUS_DUMPSYS);
    if (*state != ANDROID_WIFI_STATE_UNKNOWN) {
        return ANDROID_WIFI_STATUS_DUMPSYS;
    }

    return ANDROID_WIFI_STATUS_UNAVAILABLE;
}

static int query_wifi_enabled_setting(int *enabled)
{
    char status[64];
    ssize_t status_len = run_process_and_read_stdout((const char *[]) {"settings", "get", "global", "wifi_on", NULL}, status, sizeof(status) - 1);
    if (status_len <= 0)
        return -1;

    status[status_len] = 0;
    char *end;
    errno = 0;
    long value = strtol(status, &end, 10);
    while (*end && isspace((unsigned char) *end)) {
        end++;
    }

    if (errno || end == status || *end) {
        return -1;
    }

    // WifiSettingsStore uses 1 for enabled and 3 for enabled while overriding
    // airplane mode (0 and 2 mean disabled)
    *enabled = value == 1 || value == 3;
    return 0;
}

static int query_wifi_scan_always_setting(int *enabled)
{
    char status[64];
    ssize_t status_len = run_process_and_read_stdout((const char *[]) {"settings", "get", "global", "wifi_scan_always_enabled", NULL}, status, sizeof(status) - 1);
    if (status_len <= 0) {
        return -1;
    }

    status[status_len] = 0;
    char *end;
    errno = 0;
    long value = strtol(status, &end, 10);
    while (*end && isspace((unsigned char) *end)) {
        end++;
    }
    if (errno || end == status || *end) {
        return -1;
    }

    *enabled = value != 0;
    return 0;
}

static int set_wifi_scan_always_setting(int enabled)
{
    return run_process_and_read_stdout((const char *[]) {"settings", "put", "global", "wifi_scan_always_enabled", enabled ? "1" : "0", NULL}, NULL, 0) < 0 ? -1 : 0;
}

static void restore_android_wifi_state(void)
{
    if (restore_wifi) {
        nlprint("ANDROID: RESTORING THE SYSTEM WI-FI");
        run_process_and_read_stdout((const char *[]) {"svc", "wifi", "enable", NULL}, NULL, 0);
        restore_wifi = 0;
    }

    if (restore_wifi_scan_always) {
        nlprint("ANDROID: RESTORING BACKGROUND WI-FI SCANNING");
        set_wifi_scan_always_setting(1);
        restore_wifi_scan_always = 0;
    }
}

static int process_named_is_running(const char *name)
{
    DIR *proc = opendir("/proc");
    if (!proc) {
        return 0;
    }

    int found = 0;
    struct dirent *entry;
    while (!found && (entry = readdir(proc))) {
        if (!isdigit((unsigned char) entry->d_name[0])) {
            continue;
        }

        char comm_path[PATH_MAX];
        if (snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm", entry->d_name) >= (int) sizeof(comm_path)) {
            continue;
        }

        FILE *comm = fopen(comm_path, "r");
        if (!comm) {
            continue;
        }

        char process_name[64];
        if (fgets(process_name, sizeof(process_name), comm)) {
            process_name[strcspn(process_name, "\r\n")] = 0;
            found = !strcmp(process_name, name);
        }
        fclose(comm);
    }

    closedir(proc);
    return found;
}

static int stock_supplicant_is_running(void)
{
    return process_named_is_running("wpa_supplicant") || process_named_is_running("p2p_supplicant");
}

static int64_t monotonic_milliseconds(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
        return -1;
    }

    return (int64_t) now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int wait_for_wifi_shutdown(android_wifi_status_source_t status_source)
{
    int64_t started_at = monotonic_milliseconds();
    int64_t quiet_since = -1;
    int64_t next_status_check = started_at;
    android_wifi_state_t last_state = ANDROID_WIFI_STATE_UNKNOWN;
    int supplicant_running = 0;

    if (started_at < 0) {
        return -1;
    }

    nlprint("ANDROID: WAITING FOR THE SYSTEM WI-FI TO RELEASE WLAN");
    while (1) {
        int64_t now = monotonic_milliseconds();
        if (now < 0) {
            return -1;
        }

        if (status_source != ANDROID_WIFI_STATUS_UNAVAILABLE
            && now >= next_status_check) {
            last_state = query_wifi_state(status_source);
            now = monotonic_milliseconds();
            if (now < 0) {
                return -1;
            }
            next_status_check = now + ANDROID_WIFI_STATUS_POLL_INTERVAL_MS;
        }

        // Some Android builds may keep an idle supplicant service alive even
        // after disabling Wi-Fi. Check for it.
        supplicant_running = status_source == ANDROID_WIFI_STATUS_UNAVAILABLE && stock_supplicant_is_running();

        // Some Android builds may attempt to recover the Wi-Fi, wait to ensure
        // that doesn't happen
        int framework_disabled = status_source == ANDROID_WIFI_STATUS_UNAVAILABLE || last_state == ANDROID_WIFI_STATE_DISABLED;
        if (framework_disabled && !supplicant_running) {
            if (quiet_since < 0) {
                quiet_since = now;
            }
            if (now - quiet_since >= ANDROID_WIFI_QUIET_PERIOD_MS) {
                nlprint("ANDROID: SYSTEM WI-FI RELEASED WLAN");
                return 0;
            }
        } else {
            quiet_since = -1;
        }

        if (now - started_at >= ANDROID_WIFI_SHUTDOWN_TIMEOUT_MS) {
            break;
        }

        usleep(ANDROID_WIFI_POLL_INTERVAL_US);
    }

    if (status_source == ANDROID_WIFI_STATUS_UNAVAILABLE) {
        nlprint("ANDROID: TIMED OUT WAITING FOR THE SYSTEM WI-FI "
                "(supplicant=%s)",
                supplicant_running ? "running" : "stopped");
    } else {
        nlprint("ANDROID: TIMED OUT WAITING FOR THE SYSTEM WI-FI "
                "(state=%s)",
                last_state == ANDROID_WIFI_STATE_DISABLED ? "disabled" :
                last_state == ANDROID_WIFI_STATE_NOT_DISABLED ? "active" :
                "unknown");
    }
    return -1;
}

int vanilla_android_acquire_wifi(const char *wireless_interface)
{
    android_wifi_state_t initial_state;
    android_wifi_status_source_t status_source = find_wifi_status_source(&initial_state);
    if (query_wifi_enabled_setting(&restore_wifi) < 0) {
        restore_wifi = initial_state != ANDROID_WIFI_STATE_DISABLED;
    }

    // Android may continue doing location scans even while Wi-Fi is off,
    // conflicting with us. Ensure background scanning is also off.
    restore_wifi_scan_always = 0;
    int scan_always_enabled;
    if (query_wifi_scan_always_setting(&scan_always_enabled) == 0 &&
        scan_always_enabled) {
        nlprint("ANDROID: TEMPORARILY DISABLING BACKGROUND WI-FI SCANNING");
        if (set_wifi_scan_always_setting(0) < 0) {
            nlprint("ANDROID: FAILED TO DISABLE BACKGROUND WI-FI SCANNING");
            return -1;
        }
        restore_wifi_scan_always = 1;
    }

    nlprint("ANDROID: TEMPORARILY DISABLING THE SYSTEM WI-FI");
    if (run_process_and_read_stdout(
            (const char *[]) {"svc", "wifi", "disable", NULL}, NULL, 0) < 0) {
        nlprint("ANDROID: FAILED TO DISABLE THE SYSTEM WI-FI");
        restore_android_wifi_state();
        return -1;
    }

    if (wait_for_wifi_shutdown(status_source) < 0) {
        restore_android_wifi_state();
        return -1;
    }

    if (!if_nametoindex(wireless_interface)) {
        nlprint("ANDROID: INTERFACE %s DISAPPEARED WHEN WI-FI WAS DISABLED; "
                "THIS DEVICE NEEDS A VENDOR-SPECIFIC RADIO BACKEND",
                wireless_interface);
        restore_android_wifi_state();
        return -1;
    }

    return 0;
}

void vanilla_android_release_wifi(void)
{
    restore_android_wifi_state();
}
