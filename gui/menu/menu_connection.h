#ifndef VANILLA_PI_MENU_CONNECTION_H
#define VANILLA_PI_MENU_CONNECTION_H

#include "ui/ui.h"

void vpi_menu_connection_and_return_to(vui_context_t *vui, int fade_fglayer, vui_callback_t success_callback, void *success_data, vui_callback_t fail_callback, void *fail_data);
void vpi_menu_connection(vui_context_t *vui, void *v);

#endif // VANILLA_PI_MENU_CONNECTION_H