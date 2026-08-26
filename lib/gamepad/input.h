#ifndef GAMEPAD_INPUT_H
#define GAMEPAD_INPUT_H

#include <stdint.h>

enum StatusFlag {
    STATUS_NFC_INITIALIZED,
    STATUS_NFC_TAG_FOUND,
    STATUS_NFC_COMMAND_DONE,
    STATUS_NFC_MODE,
    STATUS_NFC_POWER_MODE,
    STATUS_NFC_CRC_DISABLED,
    STATUS_IRC_HAS_DATA,
    STATUS_IRC_CONNECTED,
    STATUS_TV_MENU,
};

void *listen_input(void *x);
void set_button_state(int button, int32_t value);
void set_touch_state(int x, int y);
void set_battery_status(int status);
void set_status_flag(enum StatusFlag flag, uint8_t status);

#endif // GAMEPAD_INPUT_H