#ifndef VANILLA_WPA_H
#define VANILLA_WPA_H

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

struct wpa_ctrl;
extern const char *wpa_ctrl_interface;

void wpa_ctrl_command(struct wpa_ctrl *ctrl, const char *cmd, char *buf, size_t *buf_len);
int start_wpa_supplicant(const char *wireless_interface, const char *config_file, pid_t *pid);

int is_networkmanager_managing_device(const char *wireless_interface, int *is_managed);
int disable_networkmanager_on_device(const char *wireless_interface);
int enable_networkmanager_on_device(const char *wireless_interface);

void nlprint(const char *fmt, ...);

void pipe_listen(int local, const char *wireless_interface, const char *log_file, const char *playback_file, const char *dump_file);

#endif // VANILLA_WPA_H
