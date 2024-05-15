#ifndef VANILLA_GAMEPAD_H
#define VANILLA_GAMEPAD_H

#include "vanilla.h"

struct wpa_ctrl;

int connect_as_gamepad_internal(struct wpa_ctrl *ctrl, const char *wireless_interface, vanilla_event_handler_t event_handler, void *context);
void set_button_state(int button_id, int pressed);
void set_axis_state(int axis_id, float value);

#endif // VANILLA_GAMEPAD_H