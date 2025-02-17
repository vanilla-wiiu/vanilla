#include "game_main.h"

#include <pthread.h>
#include <string.h>
#include <vanilla.h>

#include "game_decode.h"
#include "ui/ui.h"
#include "platform.h"

static pthread_t vpi_event_handler_thread;
static int vpi_game_error_value;
static pthread_mutex_t vpi_error_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t vpi_toast_mutex = PTHREAD_MUTEX_INITIALIZER;
static char vpi_toast_string[VPI_TOAST_MAX_LEN];
static struct timeval vpi_toast_expiry;
static int vpi_toast_number = 0;

int vpi_game_error()
{
    int r;
    pthread_mutex_lock(&vpi_error_mutex);
    r = vpi_game_error_value;
    pthread_mutex_unlock(&vpi_error_mutex);
    return r;
}

void vpi_game_set_error(int r)
{
    pthread_mutex_lock(&vpi_error_mutex);
    vpi_game_error_value = r;
    pthread_mutex_unlock(&vpi_error_mutex);
}

void *vpi_event_handler(void *data)
{
	vanilla_event_t event;

	vpi_decode_loop_running = 1;

    pthread_t decode_thread;
    pthread_create(&decode_thread, 0, vpi_decode_loop, 0);

    vui_context_t *vui = (vui_context_t *) data;

	while (vanilla_wait_event(&event)) {
        int stop = 0;

        switch (event.type) {
        case VANILLA_EVENT_VIDEO:
            pthread_mutex_lock(&vpi_decode_loop_mutex);

            vpi_decode_size = event.size;
            memcpy(vpi_decode_data, event.data, event.size);
            vpi_decode_ready = 1;

            pthread_cond_broadcast(&vpi_decode_loop_cond);
            pthread_mutex_unlock(&vpi_decode_loop_mutex);
            break;
        case VANILLA_EVENT_AUDIO:
            vui_audio_push(vui, event.data, event.size);

            // We send audio to vpi_decode, but not actually for decoding since
            // the audio is already uncompressed. We send it in case vpi_decode
            // is recording, so the audio can be written to the file.
            vpi_decode_send_audio(event.data, event.size);
            break;
        case VANILLA_EVENT_VIBRATE:
            vui_vibrate_set(vui, event.data[0]);
            break;
        case VANILLA_EVENT_ERROR:
            vpi_game_set_error(*(int *)event.data);
            break;
        }

		vanilla_free_event(&event);
	}

    pthread_mutex_lock(&vpi_decode_loop_mutex);
	vpi_decode_loop_running = 0;
	pthread_cond_broadcast(&vpi_decode_loop_cond);
    pthread_mutex_unlock(&vpi_decode_loop_mutex);

    pthread_join(decode_thread, 0);

	return 0;
}

void vpi_game_start(vui_context_t *vui)
{
    vpi_game_error_value = VANILLA_SUCCESS;
    pthread_create(&vpi_event_handler_thread, 0, vpi_event_handler, vui);
}

void vpi_show_toast(const char *message)
{
    pthread_mutex_lock(&vpi_toast_mutex);

    strncpy(vpi_toast_string, message, sizeof(vpi_toast_string) - 1);
    vpi_toast_string[sizeof(vpi_toast_string) - 1] = 0;

    gettimeofday(&vpi_toast_expiry, 0);
    
    // Toast lasts for 2 seconds
    vpi_toast_expiry.tv_sec += 2;

    vpi_toast_number++;

    pthread_mutex_unlock(&vpi_toast_mutex);
}

void vpi_get_toast(int *number, char *output, size_t output_size, struct timeval *expiry_time)
{
    pthread_mutex_lock(&vpi_toast_mutex);

    if (number)
        *number = vpi_toast_number;
    
    if (output && output_size) {
        strncpy(output, vpi_toast_string, output_size - 1);
        output[output_size-1] = 0;
    }

    if (expiry_time) {
        *expiry_time = vpi_toast_expiry;
    }

    pthread_mutex_unlock(&vpi_toast_mutex);
}