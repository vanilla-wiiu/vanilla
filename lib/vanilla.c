#include "vanilla.h"

#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <wpa_ctrl.h>

#include "gamepad/gamepad.h"
#include "gamepad/input.h"
#include "gamepad/video.h"
#include "status.h"
#include "sync.h"
#include "util.h"
#include "vanilla.h"
#include "wpa.h"

struct sync_args {
    uint16_t code;
};

struct connect_args {
    const char *wireless_interface;
    vanilla_event_handler_t event_handler;
    void *event_handler_context;
};

int thunk_to_sync(struct wpa_ctrl *ctrl, void *data)
{
    struct sync_args *args = (struct sync_args *) data;
    return sync_with_console_internal(ctrl, args->code);
}

int thunk_to_connect(struct wpa_ctrl *ctrl, void *data)
{
    struct connect_args *args = (struct connect_args *) data;
    return connect_as_gamepad_internal(ctrl, args->wireless_interface, args->event_handler, args->event_handler_context);
}

int vanilla_sync_with_console(const char *wireless_interface, uint16_t code)
{
    const char *wireless_conf_file;

    FILE *config;
    wireless_conf_file = get_wireless_authenticate_config_filename();
    config = fopen(wireless_conf_file, "w");
    if (!config) {
        print_info("FAILED TO WRITE TEMP CONFIG: %s", wireless_conf_file);
        return VANILLA_ERROR;
    }

    fprintf(config, "ctrl_interface=%s\nupdate_config=1\n", wpa_ctrl_interface);
    fclose(config);

    struct sync_args args;
    args.code = code;

    return wpa_setup_environment(wireless_interface, wireless_conf_file, thunk_to_sync, &args);
}

int vanilla_connect_to_console(const char *wireless_interface, vanilla_event_handler_t event_handler, void *context)
{
    struct connect_args args;
    args.wireless_interface = wireless_interface;
    args.event_handler = event_handler;
    args.event_handler_context = context;

    return wpa_setup_environment(wireless_interface, get_wireless_connect_config_filename(), thunk_to_connect, &args);
}

int vanilla_has_config()
{
    return (access(get_wireless_connect_config_filename(), F_OK) == 0);
}

void vanilla_stop()
{
    force_interrupt();
}

void vanilla_set_button(int button, int32_t value)
{
    set_button_state(button, value);
}

void vanilla_set_touch(int x, int y)
{
    set_touch_state(x, y);
}

void default_logger(const char *format, va_list args)
{
    vprintf(format, args);
}

void (*custom_logger)(const char *, va_list) = default_logger;
void vanilla_log(const char *format, ...)
{
    va_list va;
    va_start(va, format);

    if (custom_logger) {
        custom_logger(format, va);
        custom_logger("\n", va);
    }

    va_end(va);
}

void vanilla_log_no_newline(const char *format, ...)
{
    va_list va;
    va_start(va, format);

    vanilla_log_no_newline_va(format, va);

    va_end(va);
}

void vanilla_log_no_newline_va(const char *format, va_list args)
{
    if (custom_logger) {
        custom_logger(format, args);
    }
}

void vanilla_install_logger(void (*logger)(const char *, va_list))
{
    custom_logger = logger;
}

void vanilla_request_idr()
{
    request_idr();
}

void vanilla_retrieve_sps_pps_data(void *data, size_t *size)
{
    if (data != NULL) {
        memcpy(data, sps_pps_params, MIN(*size, sizeof(sps_pps_params)));
    }
    *size = sizeof(sps_pps_params);
}