#ifndef VANILLA_GAMEPAD_H
#define VANILLA_GAMEPAD_H

#include "vanilla.h"

#include <pthread.h>
#include <stdint.h>

extern uint16_t PORT_MSG;
extern uint16_t PORT_VID;
extern uint16_t PORT_AUD;
extern uint16_t PORT_HID;
extern uint16_t PORT_CMD;

struct wpa_ctrl;

#define VANILLA_MAX_EVENT_COUNT 20
typedef struct
{
    vanilla_event_t events[VANILLA_MAX_EVENT_COUNT];
    size_t new_index;
    size_t used_index;
    int active;
    pthread_mutex_t mutex;
    pthread_cond_t waitcond;
} event_loop_t;

typedef struct
{
    event_loop_t *event_loop;
    int socket_vid;
    int socket_aud;
    int socket_hid;
    int socket_msg;
    int socket_cmd;
} gamepad_context_t;

int sync_internal(uint16_t code, uint32_t server_address);
int connect_as_gamepad_internal(event_loop_t *ctx, uint32_t server_address);
void send_to_console(int fd, const void *data, size_t data_size, int port);
int is_stop_code(const uint8_t *data, size_t data_length);
int push_event(event_loop_t *loop, int type, const void *data, size_t size);
int get_event(event_loop_t *loop, vanilla_event_t *event, int wait);

void init_event_buffer_arena();
void free_event_buffer_arena();
void *get_event_buffer();
void release_event_buffer(void *buffer);

#endif // VANILLA_GAMEPAD_H