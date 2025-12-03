#ifndef VANILLA_PI_MENU_SUDO_WARNING_H
#define VANILLA_PI_MENU_SUDO_WARNING_H

#include "ui/ui.h"

void vpi_menu_create_sudo_warning(vui_context_t *vui, int layer, vui_button_callback_t ack_action, void *ack_action_userdata, vui_button_callback_t cancel_action, void *cancel_action_userdata);

#endif // VANILLA_PI_MENU_SUDO_WARNING_H
