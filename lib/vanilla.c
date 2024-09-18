#include "vanilla.h"

#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "gamepad/command.h"
#include "gamepad/gamepad.h"
#include "gamepad/input.h"
#include "gamepad/video.h"
#include "status.h"
#include "util.h"
#include "vanilla.h"

pthread_mutex_t main_mutex = PTHREAD_MUTEX_INITIALIZER;

int vanilla_start(vanilla_event_handler_t event_handler, void *context)
{
    if (pthread_mutex_trylock(&main_mutex) == 0) {
        int r = connect_as_gamepad_internal(event_handler, context, 0);
        pthread_mutex_unlock(&main_mutex);
        return r;
    } else {
        return VANILLA_ERROR;
    }
}

int vanilla_start_udp(vanilla_event_handler_t event_handler, void *context, uint32_t server_address)
{
    if (pthread_mutex_trylock(&main_mutex) == 0) {
        int r = connect_as_gamepad_internal(event_handler, context, server_address);
        pthread_mutex_unlock(&main_mutex);
        return r;
    } else {
        return VANILLA_ERROR;
    }
}

void vanilla_stop()
{
    // Signal to all other threads to exit gracefully
    force_interrupt();

    // Block until most recent vanilla_start finishes
    pthread_mutex_lock(&main_mutex);
    pthread_mutex_unlock(&main_mutex);
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

void vanilla_set_region(int region)
{
    set_region(region);
}

void vanilla_set_battery_status(int battery_status)
{
    set_battery_status(battery_status);
}

int vanilla_sync(uint16_t code, uint32_t server_address)
{
    return sync_internal(code, server_address);
}