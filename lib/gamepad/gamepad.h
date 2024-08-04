#ifndef VANILLA_GAMEPAD_H
#define VANILLA_GAMEPAD_H

#include "ports.h"
#include "vanilla.h"

struct wpa_ctrl;

struct gamepad_thread_context
{
    vanilla_event_handler_t event_handler;
    void *context;

    int socket_vid;
    int socket_aud;
    int socket_hid;
    int socket_msg;
    int socket_cmd;
};

int connect_as_gamepad_internal(struct wpa_ctrl *ctrl, const char *wireless_interface, vanilla_event_handler_t event_handler, void *context);
unsigned int reverse_bits(unsigned int b, int bit_count);
void send_to_console(int fd, const void *data, size_t data_size, int port);
int is_stop_code(const char *data, size_t data_length);

#endif // VANILLA_GAMEPAD_H