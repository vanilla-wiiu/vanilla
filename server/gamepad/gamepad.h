#ifndef VANILLA_GAMEPAD_H
#define VANILLA_GAMEPAD_H

#include "vanilla.h"

struct wpa_ctrl;

static const uint16_t PORT_MSG = 50110;
static const uint16_t PORT_VID = 50120;
static const uint16_t PORT_AUD = 50121;
static const uint16_t PORT_HID = 50122;
static const uint16_t PORT_CMD = 50123;

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
void set_button_state(int button_id, int pressed);
void set_axis_state(int axis_id, float value);
unsigned int reverse_bits(unsigned int b, int bit_count);
void send_to_console(int fd, const void *data, size_t data_size, int port);

#endif // VANILLA_GAMEPAD_H