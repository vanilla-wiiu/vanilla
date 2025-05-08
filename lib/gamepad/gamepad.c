#define _GNU_SOURCE

#include "gamepad.h"

#ifdef _WIN32
#include <winsock2.h>
typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;
#else
#include <arpa/inet.h>
#include <errno.h>
#include <sys/un.h>
#endif

#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "audio.h"
#include "command.h"
#include "input.h"
#include "video.h"

#include "../pipe/def.h"
#include "util.h"

static uint32_t SERVER_ADDRESS = 0;
static const int MAX_PIPE_RETRY = 5;

#define EVENT_BUFFER_SIZE 65536
#define EVENT_BUFFER_ARENA_SIZE VANILLA_MAX_EVENT_COUNT * 2
uint8_t *EVENT_BUFFER_ARENA[EVENT_BUFFER_ARENA_SIZE] = {0};
pthread_mutex_t event_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef union {
    struct sockaddr_in in;
#ifndef _WIN32
    struct sockaddr_un un;
#endif
} sockaddr_u;

static inline int skterr()
{
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

in_addr_t get_real_server_address()
{
    if (SERVER_ADDRESS == VANILLA_ADDRESS_DIRECT) {
        // The Wii U always places itself at this address
        return inet_addr("192.168.1.10");
    } else if (SERVER_ADDRESS != VANILLA_ADDRESS_LOCAL) {
        // If there's a remote pipe, we send to that
        return SERVER_ADDRESS;
    }

    // Must be local, in which case we don't care
    return INADDR_ANY;
}

void create_sockaddr(sockaddr_u *addr, size_t *size, in_addr_t inaddr, uint16_t port, int delete)
{
#ifndef _WIN32
    if (SERVER_ADDRESS == VANILLA_ADDRESS_LOCAL) {
        memset(&addr->un, 0, sizeof(addr->un));
        addr->un.sun_family = AF_UNIX;
        snprintf(addr->un.sun_path, sizeof(addr->un.sun_path) - 1, VANILLA_PIPE_LOCAL_SOCKET, port);
        if (delete)
            unlink(addr->un.sun_path);

        if (size) *size = sizeof(struct sockaddr_un);
    } else {
#endif
        memset(&addr->in, 0, sizeof(addr->in));
        addr->in.sin_family = AF_INET;
        addr->in.sin_port = htons(port);
        addr->in.sin_addr.s_addr = inaddr;
        
        if (size) *size = sizeof(struct sockaddr_in);
#ifndef _WIN32
    }
#endif
}

void send_to_console(int fd, const void *data, size_t data_size, uint16_t port)
{
    sockaddr_u addr;
    size_t addr_size;

    in_port_t console_port = port - 100;

    create_sockaddr(&addr, &addr_size, get_real_server_address(), console_port, 0);

    ssize_t sent = sendto(fd, data, data_size, 0, (const struct sockaddr *) &addr, addr_size);
    if (sent == -1) {
        vanilla_log("Failed to send to Wii U socket: fd: %d, port: %d, errno: %i", fd, console_port, skterr());
    }
}

void set_socket_rcvtimeo(int skt, uint64_t microseconds)
{
#ifdef _WIN32
    DWORD millis = microseconds / 1000;
    setsockopt(skt, SOL_SOCKET, SO_RCVTIMEO, (const char *) &millis, sizeof(millis));
#else
    struct timeval tv = {0};
    tv.tv_sec = microseconds / 1000000;
    tv.tv_usec = microseconds % 1000000;
    setsockopt(skt, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif // _WIN32
}

int create_socket(int *socket_out, in_port_t port)
{
    sockaddr_u addr;
    size_t addr_size;
    
    create_sockaddr(&addr, &addr_size, INADDR_ANY, port, 1);

    int domain = (SERVER_ADDRESS == VANILLA_ADDRESS_LOCAL) ? AF_UNIX : AF_INET;
        
    int skt = socket(domain, SOCK_DGRAM, 0);
    if (skt == -1) {
        vanilla_log("FAILED TO CREATE SOCKET: %i", skterr());
        return VANILLA_ERR_BAD_SOCKET;
    }

    if (bind(skt, (const struct sockaddr *) &addr, addr_size) == -1) {
        vanilla_log("FAILED TO BIND PORT %u: %i", port, skterr());
        close(skt);
        return VANILLA_ERR_BAD_SOCKET;
    }

    // vanilla_log("SUCCESSFULLY BOUND SOCKET %i ON PORT %i", skt, port);
    
    (*socket_out) = skt;

    set_socket_rcvtimeo(skt, 250000);
    
    return VANILLA_SUCCESS;
}

int send_pipe_cc(int skt, vanilla_pipe_command_t *cmd, size_t cmd_size, int wait_for_reply)
{
    sockaddr_u addr;
    size_t addr_size;

    create_sockaddr(&addr, &addr_size, get_real_server_address(), VANILLA_PIPE_CMD_SERVER_PORT, 0);

    ssize_t read_size;
    uint8_t recv_cc;

    for (int retries = 0; retries < MAX_PIPE_RETRY; retries++) {
        if (sendto(skt, (const char *) cmd, cmd_size, 0, (const struct sockaddr *) &addr, addr_size) == -1) {
            vanilla_log("Failed to write control code to socket");
            return 0;
        }

        if (!wait_for_reply || is_interrupted()) {
            return 1;
        }
        
        read_size = recv(skt, &recv_cc, sizeof(recv_cc), 0);
        if (recv_cc == VANILLA_PIPE_CC_BIND_ACK) {
            return 1;
        }

        vanilla_log("STILL WAITING FOR REPLY");

        sleep(1);
    }
    
    return 0;
}

int send_unbind_cc(int skt)
{
    vanilla_pipe_command_t cmd;
    cmd.control_code = VANILLA_PIPE_CC_UNBIND;
    return send_pipe_cc(skt, &cmd, sizeof(cmd.control_code), 0);
}

int connect_to_backend(int *socket, vanilla_pipe_command_t *cmd, size_t cmd_size)
{
    // Try to bind with backend
    int pipe_cc_skt = -1;
    int ret = create_socket(&pipe_cc_skt, VANILLA_PIPE_CMD_CLIENT_PORT);
    if (ret != VANILLA_SUCCESS) {
        return ret;
    }

    set_socket_rcvtimeo(pipe_cc_skt, 2000000);

    if (!send_pipe_cc(pipe_cc_skt, cmd, cmd_size, 1)) {
        vanilla_log("FAILED TO BIND TO PIPE");
        close(pipe_cc_skt);
        return VANILLA_ERR_PIPE_UNRESPONSIVE;
    }

    *socket = pipe_cc_skt;

    return VANILLA_SUCCESS;
}

void wait_for_interrupt()
{
    while (!is_interrupted()) {
        usleep(100000);
    }
}

void sync_internal(thread_data_t *data)
{
    clear_interrupt();

    SERVER_ADDRESS = data->server_address;

    uint16_t code = (uintptr_t) data->thread_data;

    vanilla_pipe_command_t cmd;
    cmd.control_code = VANILLA_PIPE_CC_SYNC;
    cmd.sync.code = htons(code);

    int skt = -1;

    vanilla_sync_event_t syncdata;

    syncdata.status = connect_to_backend(&skt, &cmd, sizeof(cmd.control_code) + sizeof(cmd.sync));

    if (syncdata.status == VANILLA_SUCCESS) {
        // Wait for sync result from pipe
        vanilla_pipe_command_t recv_cmd;
        syncdata.status = VANILLA_ERR_PIPE_UNRESPONSIVE;
        for (int retries = 0; retries < MAX_PIPE_RETRY; retries++) {
            ssize_t read_size = recv(skt, (char *) &recv_cmd, sizeof(recv_cmd), 0);

            if (recv_cmd.control_code == VANILLA_PIPE_CC_STATUS) {
                syncdata.status = (int32_t) ntohl(recv_cmd.status.status);
                break;
            } else if (recv_cmd.control_code == VANILLA_PIPE_CC_SYNC_SUCCESS) {
                syncdata.status = VANILLA_SUCCESS;
                syncdata.data.bssid = recv_cmd.connection.bssid;
                syncdata.data.psk = recv_cmd.connection.psk;
                break;
            } else if (recv_cmd.control_code == VANILLA_PIPE_CC_PING) {
                // Pipe is still responsive but hasn't found anything yet
                retries = -1;
            }

            if (is_interrupted()) {
                send_unbind_cc(skt);
                break;
            }
        }
    }

    if (syncdata.status == VANILLA_SUCCESS) {
        push_event(data->event_loop, VANILLA_EVENT_SYNC, &syncdata, sizeof(syncdata));
    } else {
        push_event(data->event_loop, VANILLA_EVENT_ERROR, &syncdata.status, sizeof(syncdata.status));
    }

    // Wait for interrupt so frontend has a chance to receive event
    wait_for_interrupt();

exit_pipe:
    if (skt != -1)
        close(skt);
}

void connect_as_gamepad_internal(thread_data_t *data)
{
    clear_interrupt();

    SERVER_ADDRESS = data->server_address;

    gamepad_context_t info;
    info.event_loop = data->event_loop;

    int ret = VANILLA_SUCCESS;

    // Open all required sockets
    if (create_socket(&info.socket_vid, PORT_VID) != VANILLA_SUCCESS) goto exit_pipe;
    if (create_socket(&info.socket_msg, PORT_MSG) != VANILLA_SUCCESS) goto exit_vid;
    if (create_socket(&info.socket_hid, PORT_HID) != VANILLA_SUCCESS) goto exit_msg;
    if (create_socket(&info.socket_aud, PORT_AUD) != VANILLA_SUCCESS) goto exit_hid;
    if (create_socket(&info.socket_cmd, PORT_CMD) != VANILLA_SUCCESS) goto exit_aud;

    int pipe_cc_skt = -1;
    if (SERVER_ADDRESS != VANILLA_ADDRESS_DIRECT) {
        vanilla_pipe_command_t cmd;
        cmd.control_code = VANILLA_PIPE_CC_CONNECT;
        
        cmd.connection.bssid = data->bssid;
        cmd.connection.psk = data->psk;
        
        // Connect to backend pipe
        ret = connect_to_backend(&pipe_cc_skt, &cmd, sizeof(cmd.control_code) + sizeof(cmd.connection));
        if (ret == VANILLA_SUCCESS) {
            // Wait for backend to be available
            vanilla_pipe_command_t connected_state;
            ret = VANILLA_ERR_NO_CONNECTION;
            while (!is_interrupted()) {
                ssize_t read_size = recv(pipe_cc_skt, (char *) &connected_state, sizeof(connected_state), 0);
                if (read_size < 0) {
                    int r = skterr();
#ifdef _WIN32
                    if (r != WSAETIMEDOUT) {
#else
                    if (r != EAGAIN) {
#endif
                        vanilla_log("FAILED TO GET CONNECTED STATE: %i", r);
                        ret = VANILLA_ERR_PIPE_UNRESPONSIVE;
                        break;
                    }
                } else if (connected_state.control_code == VANILLA_PIPE_CC_CONNECTED) {
                    ret = VANILLA_SUCCESS;
                    sleep(1);
                    break;
                }

                vanilla_log("STILL WAITING FOR CONNECTED STATE");
            }
        }
    }

    if (ret == VANILLA_SUCCESS) {
        pthread_t video_thread, audio_thread, input_thread, msg_thread, cmd_thread;

        int cnn = VANILLA_ERR_CONNECTED;
        push_event(data->event_loop, VANILLA_EVENT_ERROR, &cnn, sizeof(cnn));

        pthread_create(&video_thread, NULL, listen_video, &info);
        pthread_setname_np(video_thread, "vanilla-video");

        pthread_create(&audio_thread, NULL, listen_audio, &info);
        pthread_setname_np(audio_thread, "vanilla-audio");

        pthread_create(&input_thread, NULL, listen_input, &info);
        pthread_setname_np(input_thread, "vanilla-input");
        
        pthread_create(&cmd_thread, NULL, listen_command, &info);
        pthread_setname_np(cmd_thread, "vanilla-cmd");

        while (!is_interrupted()) {
            vanilla_pipe_command_t pipe_state;
            ssize_t read_size = recv(pipe_cc_skt, (char *) &pipe_state, sizeof(pipe_state), 0);
            if (read_size > 0) {
                switch (pipe_state.control_code) {
                case VANILLA_PIPE_CC_DISCONNECTED:
                    cnn = VANILLA_ERR_DISCONNECTED;
                    push_event(data->event_loop, VANILLA_EVENT_ERROR, &cnn, sizeof(cnn));
                    break;
                case VANILLA_PIPE_CC_CONNECTED:
                    cnn = VANILLA_ERR_CONNECTED;
                    push_event(data->event_loop, VANILLA_EVENT_ERROR, &cnn, sizeof(cnn));
                    break;
                }
            }
        }

        pthread_join(video_thread, NULL);
        pthread_join(audio_thread, NULL);
        pthread_join(input_thread, NULL);
        pthread_join(cmd_thread, NULL);
    }

exit_cmd:
    close(info.socket_cmd);

exit_aud:
    close(info.socket_aud);

exit_hid:
    close(info.socket_hid);

exit_msg:
    close(info.socket_msg);

exit_vid:
    close(info.socket_vid);

exit_pipe:
    if (pipe_cc_skt != -1) {
        // Disconnect from pipe if necessary
        send_unbind_cc(pipe_cc_skt);
        close(pipe_cc_skt);
    }

exit:
    if (ret != VANILLA_SUCCESS) {
        push_event(info.event_loop, VANILLA_EVENT_ERROR, &ret, sizeof(ret));
    }

    // Wait for interrupt so frontend has a chance to receive event
    wait_for_interrupt();
}

int push_event(event_loop_t *loop, int type, const void *data, size_t size)
{
    int ret = VANILLA_SUCCESS;

    pthread_mutex_lock(&loop->mutex);

    if (size <= EVENT_BUFFER_SIZE) {
        // Prevent rollover by skipping oldest event if necessary
        if (loop->new_index == loop->used_index + VANILLA_MAX_EVENT_COUNT) {
            vanilla_free_event(&loop->events[loop->used_index % VANILLA_MAX_EVENT_COUNT]);
            vanilla_log("SKIPPED EVENT TO PREVENT ROLLOVER (%lu > %lu + %lu)", loop->new_index, loop->used_index, VANILLA_MAX_EVENT_COUNT);
            loop->used_index++;
        }

        vanilla_event_t *ev = &loop->events[loop->new_index % VANILLA_MAX_EVENT_COUNT];

        assert(!ev->data);

        ev->data = get_event_buffer();
        if (!ev->data) {
            vanilla_log("OUT OF MEMORY FOR NEW EVENTS");
            ret = VANILLA_ERR_OUT_OF_MEMORY;
            goto exit;
        }
        
        ev->type = type;
        memcpy(ev->data, data, size);
        ev->size = size;

        loop->new_index++;

        pthread_cond_broadcast(&loop->waitcond);
    } else {
        vanilla_log("FAILED TO PUSH EVENT: wanted %lu, only had %lu. This is a bug, please report to developers.", size, EVENT_BUFFER_SIZE);
        ret = VANILLA_ERR_INVALID_ARGUMENT;
    }

exit:
    pthread_mutex_unlock(&loop->mutex);

    return ret;
}

int get_event(event_loop_t *loop, vanilla_event_t *event, int wait)
{
    int ret = 0;

    pthread_mutex_lock(&loop->mutex);

    if (loop->active) {
        if (wait) {
            while (loop->active && loop->used_index == loop->new_index) {
                pthread_cond_wait(&loop->waitcond, &loop->mutex);
            }
        }

        if (loop->active && loop->used_index < loop->new_index) {
            // Output data to pointer
            vanilla_event_t *pull_event = &loop->events[loop->used_index % VANILLA_MAX_EVENT_COUNT];

            event->type = pull_event->type;
            event->data = pull_event->data;
            event->size = pull_event->size;

            pull_event->data = NULL;

            loop->used_index++;
            ret = 1;
        }
    }

    pthread_mutex_unlock(&loop->mutex);
    
    return ret;
}

void *get_event_buffer()
{
    void *buf = NULL;

    pthread_mutex_lock(&event_buffer_mutex);
    for (size_t i = 0; i < EVENT_BUFFER_ARENA_SIZE; i++) {
        if (EVENT_BUFFER_ARENA[i]) {
            buf = EVENT_BUFFER_ARENA[i];
            EVENT_BUFFER_ARENA[i] = NULL;
            break;
    }
        }
    pthread_mutex_unlock(&event_buffer_mutex);

    return buf;
}

void release_event_buffer(void *buffer)
{
    pthread_mutex_lock(&event_buffer_mutex);
    for (size_t i = 0; i < EVENT_BUFFER_ARENA_SIZE; i++) {
        if (!EVENT_BUFFER_ARENA[i]) {
            EVENT_BUFFER_ARENA[i] = buffer;
            break;
        }
    }
    pthread_mutex_unlock(&event_buffer_mutex);
}

void init_event_buffer_arena()
{
    for (size_t i = 0; i < EVENT_BUFFER_ARENA_SIZE; i++) {
        if (!EVENT_BUFFER_ARENA[i]) {
            EVENT_BUFFER_ARENA[i] = malloc(EVENT_BUFFER_SIZE);
        } else {
            vanilla_log("CRITICAL: Buffer wasn't returned to the arena");
        }
    }
}

void free_event_buffer_arena()
{
    for (size_t i = 0; i < EVENT_BUFFER_ARENA_SIZE; i++) {
        if (EVENT_BUFFER_ARENA[i]) {
            free(EVENT_BUFFER_ARENA[i]);
            EVENT_BUFFER_ARENA[i] = NULL;
        } else {
            vanilla_log("CRITICAL: Buffer wasn't returned to the arena");
        }
    }
}