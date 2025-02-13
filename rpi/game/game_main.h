#ifndef VPI_GAME_MAIN_H
#define VPI_GAME_MAIN_H

#include "ui/ui.h"

void vpi_game_start(vui_context_t *vui);
int vpi_game_error();
void vpi_game_set_error(int r);

#endif // VPI_GAME_MAIN_H