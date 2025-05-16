#define _GNU_SOURCE

#include "vanilla.h"

#ifdef _WIN32
#include <winsock2.h>
#endif // _WIN32

#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gamepad/command.h"
#include "gamepad/gamepad.h"
#include "gamepad/input.h"
#include "gamepad/video.h"
#include "util.h"
#include "vanilla.h"

pthread_mutex_t main_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t gamepad_mutex = PTHREAD_MUTEX_INITIALIZER;
event_loop_t event_loop = {{0}, 0, 0, 0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};

void *start_event_loop(void *arg)
{
    thread_data_t *data = (thread_data_t *) arg;

#ifdef _WIN32
    {
        WSADATA wsaData;
        int r = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (r != 0) {
            char buf[100];
            vanilla_log("Failed to WSAStartup: %i", r);
            pthread_mutex_unlock(&main_mutex);
            goto exit;
        }
    }
#endif // _WIN32

    pthread_mutex_lock(&event_loop.mutex);
    init_event_buffer_arena();
    event_loop.active = 1;
    event_loop.new_index = 0;
    event_loop.used_index = 0;
    for (int i = 0; i < VANILLA_MAX_EVENT_COUNT; i++) {
        event_loop.events[i].data = NULL;
    }
    pthread_cond_broadcast(&event_loop.waitcond);
    pthread_mutex_unlock(&event_loop.mutex);

    data->event_loop = &event_loop;

    data->thread_start(data);

    free(data);

    pthread_mutex_lock(&event_loop.mutex);
    event_loop.active = 0;

    // Free any unconsumed events
    while (event_loop.used_index < event_loop.new_index) {
        // Output data to pointer
        vanilla_event_t *pull_event = &event_loop.events[event_loop.used_index % VANILLA_MAX_EVENT_COUNT];
        vanilla_free_event(pull_event);
        event_loop.used_index++;
    }

    free_event_buffer_arena();
    pthread_cond_broadcast(&event_loop.waitcond);
    pthread_mutex_unlock(&event_loop.mutex);

#ifdef _WIN32
    WSACleanup();
#endif // _WIN32

exit:
    pthread_mutex_unlock(&main_mutex);
    return 0;
}

int vanilla_start_internal(uint32_t server_address, vanilla_bssid_t bssid, vanilla_psk_t psk, thread_start_t thread_start, void *thread_data)
{
    if (pthread_mutex_trylock(&main_mutex) == 0) {
        pthread_t other;

        thread_data_t *data = malloc(sizeof(thread_data_t));
        data->server_address = server_address;
        data->thread_start = thread_start;
        data->thread_data = thread_data;
        data->bssid = bssid;
        data->psk = psk;

        // Lock event loop mutex so it can't be set to active until we're ready
        pthread_mutex_lock(&event_loop.mutex);

        // Start other thread (which will set event loop to active)
        pthread_create(&other, NULL, start_event_loop, data);
        pthread_setname_np(other, "vanilla-gamepad");

        // Wait for event loop to be set active before returning
        while (!event_loop.active) {
            pthread_cond_wait(&event_loop.waitcond, &event_loop.mutex);
        }
        pthread_mutex_unlock(&event_loop.mutex);

        return VANILLA_SUCCESS;
    } else {
        // We're already doing something
        return VANILLA_ERR_BUSY;
    }
}

int vanilla_start(uint32_t server_address, vanilla_bssid_t bssid, vanilla_psk_t psk)
{
    return vanilla_start_internal(server_address, bssid, psk, connect_as_gamepad_internal, 0);
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
    return vanilla_start_internal(server_address, (vanilla_bssid_t){.bssid = {0}}, (vanilla_psk_t){.psk = {0}}, sync_internal, (void *) (uintptr_t) code);
}

int vanilla_poll_event(vanilla_event_t *event)
{
    return get_event(&event_loop, event, 0);
}

int vanilla_wait_event(vanilla_event_t *event)
{
    return get_event(&event_loop, event, 1);
}

int vanilla_free_event(vanilla_event_t *event)
{
    if (event->data) {
        release_event_buffer(event->data);
        event->data = NULL;
    }
    return VANILLA_SUCCESS;
}

size_t vanilla_generate_sps_params(void *data, size_t data_size)
{
    return generate_sps_params(data, data_size);
}

size_t vanilla_generate_pps_params(void *data, size_t data_size)
{
    return generate_pps_params(data, data_size);
}

size_t vanilla_generate_h264_header(void *data, size_t size)
{
    return generate_h264_header(data, size);
}

void vanilla_dump_start()
{
	record_start();
}

void vanilla_dump_stop()
{
	record_stop();
}
