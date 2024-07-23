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

int get_binary_in_working_directory(const char *bin_name, char *buf, size_t buf_size)
{
    size_t path_size = get_max_path_length();
    char *path_buf = malloc(path_size);
    if (!path_buf) {
        // Failed to allocate buffer, terminate
        return -1;
    }

    // Get current working directory
    // TODO: This is Linux only and will require changes on other platforms
    ssize_t link_len = readlink("/proc/self/exe", path_buf, path_size);
    if (link_len < 0) {
        print_info("READLINK ERROR: %i", errno);
        return -1;
    }

    // Merge current working directory with wpa_supplicant name
    path_buf[link_len] = 0;
    dirname(path_buf);
    int r = snprintf(buf, path_size, "%s/%s", path_buf, bin_name);
    free(path_buf);

    return r;
}

ssize_t read_line_from_pipe(int pipe, char *buf, size_t buf_len)
{
    int attempts = 0;
    const static int max_attempts = 5;
    ssize_t read_count = 0;
    while (read_count < buf_len) {
        ssize_t this_read = read(pipe, buf + read_count, 1);
        if (this_read == 0) {
            attempts++;
            if (is_interrupted() || attempts == max_attempts) {
                return -1;
            }
            sleep(1); // Wait for more output
            continue;
        }

        attempts = 0;

        if (buf[read_count] == '\n') {
            buf[read_count] = 0;
            break;
        }

        read_count++;
    }
    return read_count;
}

int wait_for_output(int pipe, const char *expected_output)
{
    static const int max_attempts = 5;
    int nbytes, attempts = 0, success = 0;
    const int expected_len = strlen(expected_output);
    char buf[100];
    int read_count = 0;
    int ret = 0;
    do {
        // Read line from child process
        read_line_from_pipe(pipe, buf, sizeof(buf));

        print_info("SUBPROCESS %s", buf);

        // We got success message!
        if (!memcmp(buf, expected_output, expected_len)) {
            ret = 1;
            break;
        }

        // Haven't gotten success message (yet), wait and try again
    } while (attempts < max_attempts && !is_interrupted());
    
    return ret;
}

int start_wpa_supplicant(const char *wireless_interface, const char *config_file, pid_t *pid)
{
    // TODO: drc-sim has `rfkill unblock wlan`, should we do that too?

    // Kill any potentially orphaned wpa_supplicant_drcs
    const char *wpa_supplicant_drc = "wpa_supplicant_drc";
    const char *kill_argv[] = {"killall", "-9", wpa_supplicant_drc, NULL};
    pid_t kill_pid;
    int kill_pipe;
    int r = start_process(kill_argv, &kill_pid, &kill_pipe, NULL);
    int status;
    waitpid(kill_pid, &status, 0);

    size_t path_size = get_max_path_length();
    char *wpa_buf = malloc(path_size);

    get_binary_in_working_directory(wpa_supplicant_drc, wpa_buf, path_size);

    const char *argv[] = {wpa_buf, "-Dnl80211", "-i", wireless_interface, "-c", config_file, NULL};
    int pipe;

    r = start_process(argv, pid, &pipe, NULL);
    free(wpa_buf);

    if (r != VANILLA_SUCCESS) {
        return r;
    }

    // Wait for WPA supplicant to start
    if (wait_for_output(pipe, "Successfully initialized wpa_supplicant")) {
        // I'm not sure why, but closing this pipe breaks wpa_supplicant in subtle ways, so just leave it.
        //close(pipe);

        // WPA initialized correctly! Continue with action...
        return VANILLA_SUCCESS;
    } else {
        // Give up
give_up:
        kill((*pid), SIGTERM);
        return VANILLA_ERROR;
    }
}

int wpa_setup_environment(const char *wireless_interface, const char *wireless_conf_file, ready_callback_t callback, void *callback_data)
{
    int ret = VANILLA_ERROR;

    clear_interrupt();
    //install_interrupt_handler();

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
    kill(pid, SIGTERM);

    free(buf);

die_and_reenable_managed:
    if (is_managed) {
        print_info("SETTING %s BACK TO MANAGED", wireless_interface);
        enable_networkmanager_on_device(wireless_interface);
    }

die:
    // Remove our custom sigint signal handler
    //uninstall_interrupt_handler();

    return ret;
}

int call_dhcp(const char *network_interface, pid_t *dhclient_pid)
{
    const char *argv[] = {"dhclient", "-d", "--no-pid", network_interface, NULL, NULL, NULL};

    size_t buf_size = get_max_path_length();
    char *dhclient_buf = malloc(buf_size);
    char *dhclient_script = NULL;
    get_binary_in_working_directory("dhclient", dhclient_buf, buf_size);

    if (access(dhclient_buf, F_OK) == 0) {
        // HACK: Assume we're working in our deployed environment
        // TODO: Should probably just incorporate dhclient (or something like it) directly as a library
        argv[0] = dhclient_buf;
        argv[4] = "-sf";

        dhclient_script = malloc(buf_size);
        get_binary_in_working_directory("../sbin/dhclient-script", dhclient_script, buf_size);
        argv[5] = dhclient_script;
        
        print_info("Using custom dhclient at: %s", argv[0]);
    } else {
        print_info("Using system dhclient");
    }

    int dhclient_pipe;
    int r = start_process(argv, dhclient_pid, NULL, &dhclient_pipe);
    if (r != VANILLA_SUCCESS) {
        print_info("FAILED TO CALL DHCLIENT");
        return r;
    }

    free(dhclient_buf);
    if (dhclient_script) free(dhclient_script);

    if (wait_for_output(dhclient_pipe, "bound to")) {
        return VANILLA_SUCCESS;
    } else {
        print_info("FAILED TO ESTABLISH DHCP");
        kill(*dhclient_pid, SIGTERM);

        return VANILLA_ERROR;
    }
}

static const char *nmcli = "nmcli";
int is_networkmanager_managing_device(const char *wireless_interface, int *is_managed)
{
    pid_t nmcli_pid;
    int pipe;

    const char *argv[] = {nmcli, "device", "show", wireless_interface, NULL};

    int r = start_process(argv, &nmcli_pid, &pipe, NULL);
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
    int r = start_process(argv, &nmcli_pid, NULL, NULL);
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