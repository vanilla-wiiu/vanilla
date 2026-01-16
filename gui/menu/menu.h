#ifndef VANILLA_PI_MENU_H
#define VANILLA_PI_MENU_H

#include "ui/ui.h"

typedef enum {
    VPI_ACTION_START_INDEX = 10000,
    VPI_ACTION_SCREENSHOT,
    VPI_ACTION_TOGGLE_RECORDING,
    VPI_ACTION_DISCONNECT,
    VPI_ACTION_END_INDEX
} vpi_extra_action_t;

void vpi_menu_init(vui_context_t *vui);

void vpi_menu_action(vui_context_t *vui, vpi_extra_action_t action);

#endif // VANILLA_PI_MENU_H