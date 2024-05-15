#ifndef VANILLA_SYNC_H
#define VANILLA_SYNC_H

#include <stdint.h>

struct wpa_ctrl;

int sync_with_console_internal(struct wpa_ctrl *ctrl, const char *wireless_interface, uint16_t code);

#endif // VANILLA_SYNC_H