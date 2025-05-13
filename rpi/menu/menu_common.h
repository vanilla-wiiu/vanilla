#ifndef VANILLA_MENU_COMMON_H
#define VANILLA_MENU_COMMON_H

#include "platform.h"
#include "ui/ui.h"

#define BTN_SZ 80

void vpi_menu_create_background(vui_context_t *vui, int layer, vui_rect_t *bkg_rect, int *margin);
void vpi_menu_create_back_button(vui_context_t *vui, int layer, vui_button_callback_t action, void *action_data);

void vpi_menu_start_pipe(vui_context_t *vui, int fade_fglayer, vui_callback_t success_action, void *success_data, vui_callback_t cancel_action, void *cancel_data);
int vpi_menu_show_error(vui_context_t *vui, int status, int fade_fglayer, vui_button_callback_t ok_action, void *ok_data);

void vpi_menu_quit_vanilla(vui_context_t *vui);

#endif // VANILLA_MENU_COMMON_H