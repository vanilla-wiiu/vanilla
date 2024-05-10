#ifndef VANILLA_WPA_H
#define VANILLA_WPA_H

#include <stdlib.h>

struct wpa_ctrl;

void wpa_ctrl_command(struct wpa_ctrl *ctrl, const char *cmd, char *buf, size_t *buf_len);
int start_wpa_supplicant(const char *wireless_interface, const char *config_file, pid_t *pid);

#endif // VANILLA_WPA_H