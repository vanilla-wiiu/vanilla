#ifndef GAMEPAD_INPUT_H
#define GAMEPAD_INPUT_H

#include <stdint.h>

void *listen_input(void *x);
void set_button_state(int button, int32_t value);
void set_touch_state(int x, int y);
void set_battery_status(int status);

#endif // GAMEPAD_INPUT_H