#include "wpa.h"

#include <errno.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wpa_ctrl.h>

#include "status.h"
#include "util.h"
#include "vanilla.h"

const char *wpa_ctrl_interface = "/var/run/wpa_supplicant_drc";

void wpa_msg(char *msg, size_t len)
{
    print_info("%.*s", len, msg);
}

void wpa_ctrl_command(struct wpa_ctrl *ctrl, const char *cmd, char *buf, size_t *buf_len)
{
    wpa_ctrl_request(ctrl, cmd, strlen(cmd), buf, buf_len, NULL /*wpa_msg*/);
}

int start_wpa_supplicant(const char *wireless_interface, const char *config_file, pid_t *pid)
{
    // TODO: drc-sim has `rfkill unblock wlan`, should we do that too?

    // Get path to `wpa_supplicant_drc` (assumes it's in the same path as us - presumably /usr/bin/ or equivalent)
    size_t path_size = get_max_path_length();
    char *path_buf = malloc(path_size);
    char *wpa_buf = malloc(path_size);
    if (!path_buf || !wpa_buf) {
        // Failed to allocate buffer, terminate
        return VANILLA_ERROR;
    }

    // Get current working directory
    // if (!getcwd(path_buf, path_size)) {
    //     return VANILLA_ERROR;
    // }
    // TODO: This is Linux only and will require changes on other platforms
    ssize_t link_len = readlink("/proc/self/exe", path_buf, path_size);
    if (link_len < 0) {
        print_info("READLINK ERROR: %i", errno);
        return VANILLA_ERROR;
    }

    // Merge current working directory with wpa_supplicant name
    path_buf[link_len] = 0;
    dirname(path_buf);
    snprintf(wpa_buf, path_size, "%s/%s", path_buf, "wpa_supplicant_drc");
    print_info(wpa_buf);
    free(path_buf);

    const char *argv[] = {wpa_buf, "-Dnl80211", "-i", wireless_interface, "-c", config_file, NULL};
    int pipe;

    int r = start_process(argv, pid, &pipe);
    free(wpa_buf);

    if (r != VANILLA_SUCCESS) {
        return r;
    }

    // Wait for WPA supplicant to start
    static const int max_attempts = 5;
    int nbytes, attempts = 0, success = 0, total_nbytes = 0;
    static const char *expected = "Successfully initialized wpa_supplicant";
    static const int expected_len = 39; //strlen(expected)
    char buf[expected_len];
    do {
        // Read string from child process
        do {
            nbytes = read(pipe, buf + total_nbytes, expected_len - total_nbytes);
            total_nbytes += nbytes;
            sleep(1);
            if (is_interrupted()) goto give_up;
        } while (total_nbytes < expected_len);

        attempts++;

        // We got success message!
        if (!strncmp(buf, expected, expected_len)) {
            success = 1;
            break;
        }

        // Haven't gotten success message (yet), wait and try again
        sleep(1);
        if (is_interrupted()) goto give_up;
    } while (attempts < max_attempts);

    // I'm not sure why, but closing this pipe breaks wpa_supplicant in subtle ways, so just leave it.
    //close(pipe);

    if (success) {
        // WPA initialized correctly! Continue with action...
        return VANILLA_SUCCESS;
    } else {
        // Give up
give_up:
        kill((*pid), SIGINT);
        return VANILLA_ERROR;
    }
}

int wpa_setup_environment(const char *wireless_interface, const char *wireless_conf_file, ready_callback_t callback, void *callback_data)
{
    int ret = VANILLA_ERROR;

    install_interrupt_handler();

    // Check status of interface with NetworkManager
    int is_managed = 0;
    if (is_networkmanager_managing_device(wireless_interface, &is_managed) != VANILLA_SUCCESS) {
        print_info("FAILED TO DETERMINE MANAGED STATE OF WIRELESS INTERFACE");
        //goto die;
    }

    // If NetworkManager is managing this device, temporarily stop it from doing so
    if (is_managed) {
        if (disable_networkmanager_on_device(wireless_interface) != VANILLA_SUCCESS) {
            print_info("FAILED TO SET %s TO UNMANAGED, RESULTS MAY BE UNPREDICTABLE");
        } else {
            print_info("TEMPORARILY SET %s TO UNMANAGED", wireless_interface);
        }
    }

    // Start modified WPA supplicant
    pid_t pid;
    int err = start_wpa_supplicant(wireless_interface, wireless_conf_file, &pid);
    if (err != VANILLA_SUCCESS || is_interrupted()) {
        print_info("FAILED TO START WPA SUPPLICANT");
        goto die_and_reenable_managed;
    }

    // Get control interface
    const size_t buf_len = 1048576;
    char *buf = malloc(buf_len);
    snprintf(buf, buf_len, "%s/%s", wpa_ctrl_interface, wireless_interface);
    struct wpa_ctrl *ctrl;
    while (!(ctrl = wpa_ctrl_open(buf))) {
        if (is_interrupted()) goto die_and_kill;
        print_info("WAITING FOR CTRL INTERFACE");
        sleep(1);
    }

    if (is_interrupted() || wpa_ctrl_attach(ctrl) < 0) {
        print_info("FAILED TO ATTACH TO WPA");
        goto die_and_close;
    }

    ret = callback(ctrl, callback_data);

die_and_detach:
    wpa_ctrl_detach(ctrl);

die_and_close:
    wpa_ctrl_close(ctrl);

die_and_kill:
    kill(pid, SIGINT);

    free(buf);

die_and_reenable_managed:
    if (is_managed) {
        print_info("SETTING %s BACK TO MANAGED", wireless_interface);
        enable_networkmanager_on_device(wireless_interface);
    }

die:
    // Remove our custom sigint signal handler
    uninstall_interrupt_handler();

    return ret;
}

int call_dhcp(const char *network_interface)
{
    const char *argv[] = {"dhclient", network_interface, NULL};
    pid_t dhclient_pid;
    int r = start_process(argv, &dhclient_pid, NULL);
    if (r != VANILLA_SUCCESS) {
        print_info("FAILED TO CALL DHCLIENT");
        return r;
    }

    int status;
    waitpid(dhclient_pid, &status, 0);

    if (!WIFEXITED(status)) {
        // Something went wrong
        print_info("DHCLIENT DID NOT EXIT NORMALLY");
        return VANILLA_ERROR;
    }

    return VANILLA_SUCCESS;
}

static const char *nmcli = "nmcli";
int is_networkmanager_managing_device(const char *wireless_interface, int *is_managed)
{
    pid_t nmcli_pid;
    int pipe;

    const char *argv[] = {nmcli, "device", "show", wireless_interface, NULL};

    int r = start_process(argv, &nmcli_pid, &pipe);
    if (r != VANILLA_SUCCESS) {
        // Assume nmcli is not installed so the host is not using NetworkManager
        print_info("FAILED TO LAUNCH NMCLI, RESULTS MAY BE UNPREDICTABLE");
        *is_managed = 0;
        return VANILLA_SUCCESS;
    }

    int status;
    waitpid(nmcli_pid, &status, 0);

    if (!WIFEXITED(status)) {
        // Something went wrong
        print_info("NMCLI DID NOT EXIT NORMALLY");
        return VANILLA_ERROR;
    }

    char buf[100];
    int ret = VANILLA_ERROR;
    while (read_line_from_fd(pipe, buf, sizeof(buf))) {
        if (memcmp(buf, "GENERAL.STATE", 13) == 0) {
            *is_managed = !strstr(buf, "unmanaged");
            ret = VANILLA_SUCCESS;
            goto exit;
        }
    }

exit:
    close(pipe);
    return ret;
}

int set_networkmanager_on_device(const char *wireless_interface, int on)
{
    const char *argv[] = {nmcli, "device", "set", wireless_interface, "managed", on ? "on" : "off", NULL};

    pid_t nmcli_pid;
    int r = start_process(argv, &nmcli_pid, NULL);
    if (r != VANILLA_SUCCESS) {
        return r;
    }

    int status;
    waitpid(nmcli_pid, &status, 0);
    if (WIFEXITED(status)) {
        return VANILLA_SUCCESS;
    } else {
        return VANILLA_ERROR;
    }
}

int disable_networkmanager_on_device(const char *wireless_interface)
{
    return set_networkmanager_on_device(wireless_interface, 0);
}

int enable_networkmanager_on_device(const char *wireless_interface)
{
    return set_networkmanager_on_device(wireless_interface, 1);
}