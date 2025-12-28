#ifndef VANILLA_PI_MENU_GAME_H
#define VANILLA_PI_MENU_GAME_H

#include <libavutil/frame.h>
#include <pthread.h>
#include <sys/time.h>

#include "ui/ui.h"

#define VPI_TOAST_MAX_LEN 1024

extern AVFrame *vpi_present_frame;
extern pthread_mutex_t vpi_present_frame_mutex;
extern int vpi_egl_available;

void vpi_menu_game(vui_context_t *vui, void *v);

void vpi_game_shutdown();

void vpi_get_toast(int *number, char *output, size_t output_size, struct timeval *expiry_time);
void vpi_show_toast(const char *message);

void vpi_decode_screenshot(const char *filename);
int vpi_decode_is_recording();
int vpi_decode_record(const char *filename);
void vpi_decode_record_stop();
void vpi_decode_send_audio(const void *data, size_t size);

#endif // VANILLA_PI_MENU_GAME_H
