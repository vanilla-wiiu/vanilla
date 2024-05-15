#include "vanilla.h"

#include <signal.h>
#include <unistd.h>
#include <wpa_ctrl.h>

#include "gamepad.h"
#include "status.h"
#include "sync.h"
#include "util.h"
#include "vanilla.h"
#include "wpa.h"

#define MODE_SYNC 0
#define MODE_CONNECT 1

int setup_environment(const char *wireless_interface, uint16_t code, int mode, vanilla_event_handler_t event_handler, void *context)
{
    int ret = VANILLA_ERROR;

    install_interrupt_handler();

    // Check status of interface with NetworkManager
    int is_managed;
    if (is_networkmanager_managing_device(wireless_interface, &is_managed) != VANILLA_SUCCESS) {
        print_info("FAILED TO DETERMINE MANAGED STATE OF WIRELESS INTERFACE");
        goto die;
    }

    // If NetworkManager is managing this device, temporarily stop it from doing so
    if (is_managed) {
        if (disable_networkmanager_on_device(wireless_interface) != VANILLA_SUCCESS) {
            print_info("FAILED TO SET %s TO UNMANAGED, RESULTS MAY BE UNPREDICTABLE");
        } else {
            print_info("TEMPORARILY SET %s TO UNMANAGED", wireless_interface);
        }
    }

    const char *wireless_conf_file;
    if (mode == MODE_SYNC) {
        FILE *config;
        wireless_conf_file = get_wireless_authenticate_config_filename();
        config = fopen(wireless_conf_file, "w");
        if (!config) {
            print_info("FAILED TO WRITE TEMP CONFIG");
            goto die_and_reenable_managed;
        }
        fprintf(config, "ctrl_interface=%s\nupdate_config=1\n", wpa_ctrl_interface);
        fclose(config);
    } else {
        wireless_conf_file = get_wireless_connect_config_filename();
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

    if (mode == MODE_SYNC) {
        ret = sync_with_console_internal(ctrl, wireless_interface, code);
    } else if (mode == MODE_CONNECT) {
        ret = connect_as_gamepad_internal(ctrl, wireless_interface, event_handler, context);
    }

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

int vanilla_sync_with_console(const char *wireless_interface, uint16_t code)
{
    return setup_environment(wireless_interface, code, MODE_SYNC, NULL, NULL);
}

int vanilla_connect_to_console(const char *wireless_interface, vanilla_event_handler_t event_handler, void *context)
{
    return setup_environment(wireless_interface, 0, MODE_CONNECT, event_handler, context);
}

void vanilla_stop()
{
    force_interrupt();
}

void vanilla_set_button(int button_id, int pressed)
{
    set_button_state(button_id, pressed);
}

void vanilla_set_axis(int axis_id, float value)
{
    set_axis_state(axis_id, value);
}
