#ifndef VANILLA_GAMEPAD_H
#define VANILLA_GAMEPAD_H

#include "vanilla.h"

#include <stdint.h>

extern uint16_t PORT_MSG;
extern uint16_t PORT_VID;
extern uint16_t PORT_AUD;
extern uint16_t PORT_HID;
extern uint16_t PORT_CMD;

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

int connect_as_gamepad_internal(vanilla_event_handler_t event_handler, void *context, uint32_t server_address);
unsigned int reverse_bits(unsigned int b, int bit_count);
void send_to_console(int fd, const void *data, size_t data_size, int port);
int is_stop_code(const char *data, size_t data_length);

#endif // VANILLA_GAMEPAD_H