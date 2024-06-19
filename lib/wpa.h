#ifndef VANILLA_WPA_H
#define VANILLA_WPA_H

#include <stdlib.h>
#include <sys/types.h>

struct wpa_ctrl;
extern const char *wpa_ctrl_interface;

typedef int (*ready_callback_t)(struct wpa_ctrl *, void *);

int wpa_setup_environment(const char *wireless_interface, const char *wireless_conf_file, ready_callback_t callback, void *callback_data);

void wpa_ctrl_command(struct wpa_ctrl *ctrl, const char *cmd, char *buf, size_t *buf_len);
int start_wpa_supplicant(const char *wireless_interface, const char *config_file, pid_t *pid);

int call_dhcp(const char *network_interface);

int is_networkmanager_managing_device(const char *wireless_interface, int *is_managed);
int disable_networkmanager_on_device(const char *wireless_interface);
int enable_networkmanager_on_device(const char *wireless_interface);

#endif // VANILLA_WPA_H
