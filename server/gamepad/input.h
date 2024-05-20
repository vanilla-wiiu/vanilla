#ifndef GAMEPAD_INPUT_H
#define GAMEPAD_INPUT_H

void *listen_input(void *x);
void set_button_state(int button_id, int pressed);
void set_axis_state(int axis_id, float value);
void set_touch_state(int x, int y);

#endif // GAMEPAD_INPUT_H