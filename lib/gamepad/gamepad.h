#ifndef VANILLA_GAMEPAD_H
#define VANILLA_GAMEPAD_H

#ifdef _WIN32
#include <winsock2.h>
typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;
#else
#include <arpa/inet.h>
#include <errno.h>
#include <sys/un.h>
#endif

#include "vanilla.h"

#include <pthread.h>
#include <stdint.h>

#include "../pipe/ports.h"

struct wpa_ctrl;

#define VANILLA_MAX_EVENT_COUNT 100
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

typedef struct thread_data_t thread_data_t;
typedef void (*thread_start_t)(thread_data_t *);
typedef struct thread_data_t
{
    uint32_t server_address;
    event_loop_t *event_loop;
    thread_start_t thread_start;
    void *thread_data;
    vanilla_bssid_t bssid;
    vanilla_psk_t psk;
} thread_data_t;

typedef union {
    struct sockaddr_in in;
#ifndef _WIN32
    struct sockaddr_un un;
#endif
} sockaddr_u;

void sync_internal(thread_data_t *data);
void connect_as_gamepad_internal(thread_data_t *data);
int install_polkit_internal(thread_data_t *data, int install);
void create_sockaddr(sockaddr_u *addr, size_t *size, in_addr_t inaddr, uint16_t port, int delete);
void create_server_sockaddr(sockaddr_u *addr, size_t *size, uint16_t port, int delete);
void send_to_sockaddr(int fd, const void *data, size_t data_size, const sockaddr_u *sockaddr, size_t sockaddr_size);
void send_to_console(int fd, const void *data, size_t data_size, uint16_t port);
int push_event(event_loop_t *loop, int type, const void *data, size_t size);
int get_event(event_loop_t *loop, vanilla_event_t *event, int wait);
int acquire_event(event_loop_t *loop, vanilla_event_t **event);
int release_event(event_loop_t *loop);

void init_event_buffer_arena();
void free_event_buffer_arena();
void *get_event_buffer();
void release_event_buffer(void *buffer);

#endif // VANILLA_GAMEPAD_H
