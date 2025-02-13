// TEMP: Remove this...
#include <libavutil/avutil.h>

#include <pthread.h>
#include <vanilla.h>

#include "game_decode.h"
#include "ui/ui.h"

static pthread_t vpi_event_handler_thread;
static int vpi_game_error_value;
static pthread_mutex_t vpi_error_mutex = PTHREAD_MUTEX_INITIALIZER;

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

		if (event.type == VANILLA_EVENT_VIDEO) {
            pthread_mutex_lock(&vpi_decode_loop_mutex);

            vpi_decode_size = event.size;
			memcpy(vpi_decode_data, event.data, event.size);
            vpi_decode_ready = 1;

            pthread_cond_broadcast(&vpi_decode_loop_cond);
            pthread_mutex_unlock(&vpi_decode_loop_mutex);
		} else if (event.type == VANILLA_EVENT_AUDIO) {
            vui_audio_push(vui, event.data, event.size);
		} else if (event.type == VANILLA_EVENT_VIBRATE) {
            vui_vibrate_set(vui, event.data[0]);
		} else if (event.type == VANILLA_EVENT_ERROR) {
            vpi_game_set_error(*(int *)event.data);
            stop = 1;
        }

		vanilla_free_event(&event);

        if (stop) {
            vanilla_stop();
            break;
        }
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