#ifndef VPI_GAME_MAIN_H
#define VPI_GAME_MAIN_H

#include <sys/time.h>

#include "ui/ui.h"

#define VPI_TOAST_MAX_LEN 1024

void vpi_game_start(vui_context_t *vui);
int vpi_game_error();
void vpi_game_set_error(int r);

void vpi_get_toast(int *number, char *output, size_t output_size, struct timeval *expiry_time);
void vpi_show_toast(const char *message);

#endif // VPI_GAME_MAIN_H