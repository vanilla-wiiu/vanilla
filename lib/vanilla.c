#include "vanilla.h"

#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
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
pthread_mutex_t gamepad_mutex = PTHREAD_MUTEX_INITIALIZER;
event_loop_t event_loop = {{0}, 0, 0, 0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};

struct gamepad_data_t
{
    uint32_t server_address;
};

void *start_gamepad(void *arg)
{
    struct gamepad_data_t *data = (struct gamepad_data_t *) arg;

    pthread_mutex_lock(&event_loop.mutex);
    event_loop.active = 1;
    event_loop.new_index = 0;
    event_loop.used_index = 0;
    pthread_cond_broadcast(&event_loop.waitcond);
    pthread_mutex_unlock(&event_loop.mutex);
    
    connect_as_gamepad_internal(&event_loop, data->server_address);

    free(data);

    pthread_mutex_lock(&event_loop.mutex);
    event_loop.active = 0;
    pthread_cond_broadcast(&event_loop.waitcond);
    pthread_mutex_unlock(&event_loop.mutex);

    pthread_mutex_unlock(&main_mutex);
    return 0;
}

int vanilla_start_internal(uint32_t server_address)
{
    if (pthread_mutex_trylock(&main_mutex) == 0) {
        pthread_t other;
        
        struct gamepad_data_t *data = malloc(sizeof(struct gamepad_data_t));
        data->server_address = server_address;

        // Lock event loop mutex so it can't be set to active until we're ready
        pthread_mutex_lock(&event_loop.mutex);

        // Start other thread (which will set event loop to active)
        pthread_create(&other, NULL, start_gamepad, data);

        // Wait for event loop to be set active before returning
        while (!event_loop.active) {
            pthread_cond_wait(&event_loop.waitcond, &event_loop.mutex);
        }
        pthread_mutex_unlock(&event_loop.mutex);

        return VANILLA_SUCCESS;
    } else {
        return VANILLA_ERROR;
    }
}

int vanilla_start(uint32_t server_address)
{
    return vanilla_start_internal(server_address);
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

int vanilla_poll_event(vanilla_event_t *event)
{
    return get_event(&event_loop, event, 0);
}

int vanilla_wait_event(vanilla_event_t *event)
{
    return get_event(&event_loop, event, 1);
}