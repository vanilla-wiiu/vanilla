#define _GNU_SOURCE

#include "gamepad.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <unistd.h>

#include "audio.h"
#include "command.h"
#include "input.h"
#include "video.h"

#include "../pipe/def.h"
#include "status.h"
#include "util.h"

static const uint32_t STOP_CODE = 0xCAFEBABE;
static uint32_t SERVER_ADDRESS = 0;
static const int MAX_PIPE_RETRY = 5;

#define EVENT_BUFFER_SIZE 65536
#define EVENT_BUFFER_ARENA_SIZE VANILLA_MAX_EVENT_COUNT * 2
uint8_t *EVENT_BUFFER_ARENA[EVENT_BUFFER_ARENA_SIZE] = {0};
pthread_mutex_t event_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

in_addr_t get_real_server_address()
{
    if (SERVER_ADDRESS == VANILLA_ADDRESS_DIRECT) {
        // The Wii U always places itself at this address
        return inet_addr("192.168.1.10");
    } else if (SERVER_ADDRESS != VANILLA_ADDRESS_LOCAL) {
        // If there's a remote pipe, we send to that
        return htonl(SERVER_ADDRESS);
    }

    // Must be local, in which case we don't care
    return INADDR_ANY;
}

void create_sockaddr(struct sockaddr_in *in, struct sockaddr_un *un, const struct sockaddr **addr, size_t *size, in_addr_t inaddr, uint16_t port)
{
    if (SERVER_ADDRESS == VANILLA_ADDRESS_LOCAL) {
        memset(un, 0, sizeof(struct sockaddr_un));
        un->sun_family = AF_UNIX;
        snprintf(un->sun_path, sizeof(un->sun_path) - 1, VANILLA_PIPE_LOCAL_SOCKET, port);

        if (size) *size = sizeof(struct sockaddr_un);
        if (addr) *addr = (const struct sockaddr *) un;
    } else {
        memset(in, 0, sizeof(struct sockaddr_in));
        in->sin_family = AF_INET;
        in->sin_port = htons(port);
        in->sin_addr.s_addr = inaddr;
        
        if (size) *size = sizeof(struct sockaddr_in);
        if (addr) *addr = (const struct sockaddr *) in;
    }
}

void send_to_console(int fd, const void *data, size_t data_size, uint16_t port)
{
    struct sockaddr_in in;
    struct sockaddr_un un;
    const struct sockaddr *addr;
    size_t addr_size;

    in_port_t console_port = port - 100;

    create_sockaddr(&in, &un, &addr, &addr_size, get_real_server_address(), console_port);

    ssize_t sent = sendto(fd, data, data_size, 0, addr, addr_size);
    if (sent == -1) {
        print_info("Failed to send to Wii U socket: fd: %d, port: %d, errno: %i", fd, console_port, errno);
    }
}

int create_socket(int *socket_out, in_port_t port)
{
    int skt = -1;
    struct sockaddr_un un;
    struct sockaddr_in in;
    
    create_sockaddr(&in, &un, 0, 0, INADDR_ANY, port);
    
    if (SERVER_ADDRESS == VANILLA_ADDRESS_LOCAL) {
        skt = socket(AF_UNIX, SOCK_STREAM, 0);
        if (skt == -1) {
            print_info("FAILED TO CREATE SOCKET: %i", errno);
            return VANILLA_ERR_BAD_SOCKET;
        }

        if (connect(skt, (const struct sockaddr *) &un, sizeof(un)) == -1) {
            print_info("FAILED TO CONNECT TO LOCAL SOCKET %u: %i", port, errno);
            close(skt);
            return VANILLA_ERR_PIPE_UNRESPONSIVE;
        }

        print_info("SUCCESSFULLY CONNECTED SOCKET TO PATH %s", un.sun_path);
    } else {
        in.sin_addr.s_addr = INADDR_ANY;
        skt = socket(AF_INET, SOCK_DGRAM, 0);
        if (skt == -1) {
            print_info("FAILED TO CREATE SOCKET: %i", errno);
            return VANILLA_ERR_BAD_SOCKET;
        }

        if (bind(skt, (const struct sockaddr *) &in, sizeof(in)) == -1) {
            print_info("FAILED TO BIND PORT %u: %i", port, errno);
            close(skt);
            return VANILLA_ERR_BAD_SOCKET;
        }

        print_info("SUCCESSFULLY BOUND SOCKET ON PORT %i", port);
    }
    
    (*socket_out) = skt;
    
    return VANILLA_SUCCESS;
}

void send_stop_code(int from_socket, in_port_t port)
{
    struct sockaddr_in in;
    struct sockaddr_un un;
    const struct sockaddr *addr;
    size_t addr_size;

    // FIXME: There are probably better ways of stopping the sockets than this

    create_sockaddr(&in, &un, &addr, &addr_size, inet_addr("127.0.0.1"), htons(port));
    sendto(from_socket, &STOP_CODE, sizeof(STOP_CODE), 0, addr, addr_size);
}

int send_pipe_cc(int skt, uint32_t cc, int wait_for_reply)
{
    struct sockaddr_in in;
    struct sockaddr_un un;
    const struct sockaddr *addr;
    size_t addr_size;

    create_sockaddr(&in, &un, &addr, &addr_size, get_real_server_address(), VANILLA_PIPE_CMD_PORT);

    ssize_t read_size;
    uint32_t send_cc = htonl(cc);
    uint32_t recv_cc;

    for (int retries = 0; retries < MAX_PIPE_RETRY; retries++) {
        // sendto(skt, &send_cc, sizeof(send_cc), 0, addr, addr_size);
        if (write(skt, &send_cc, sizeof(send_cc)) == -1) {
            print_info("Failed to write control code to socket");
            return 0;
        }

        if (!wait_for_reply || is_interrupted()) {
            return 1;
        }
        
        read_size = recv(skt, &recv_cc, sizeof(recv_cc), 0);
        if (read_size == sizeof(recv_cc) && ntohl(recv_cc) == VANILLA_PIPE_CC_BIND_ACK) {
            return 1;
        }

        print_info("STILL WAITING FOR REPLY");

        sleep(1);
    }
    
    return 0;
}

int connect_to_backend(int *socket, uint32_t cc)
{
    // Try to bind with backend
    int pipe_cc_skt = -1;
    int ret = create_socket(&pipe_cc_skt, VANILLA_PIPE_CMD_PORT);
    if (ret != VANILLA_SUCCESS) {
        return ret;
    }

    struct timeval tv = {0};
    tv.tv_sec = 2;
    setsockopt(pipe_cc_skt, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (!send_pipe_cc(pipe_cc_skt, cc, 1)) {
        print_info("FAILED TO BIND TO PIPE");
        close(pipe_cc_skt);
        return VANILLA_ERR_PIPE_UNRESPONSIVE;
    }

    *socket = pipe_cc_skt;

    return VANILLA_SUCCESS;
}

void sync_internal(thread_data_t *data)
{
    clear_interrupt();

    SERVER_ADDRESS = data->server_address;

    uint16_t code = (uintptr_t) data->thread_data;

    int skt = -1;
    int ret = connect_to_backend(&skt, VANILLA_PIPE_SYNC_CODE(code));

    if (ret == VANILLA_SUCCESS) {
        // Wait for sync result from pipe
        uint32_t recv_cc;
        ret = VANILLA_ERR_PIPE_UNRESPONSIVE;
        for (int retries = 0; retries < MAX_PIPE_RETRY; retries++) {
            ssize_t read_size = recv(skt, &recv_cc, sizeof(recv_cc), 0);
            if (read_size == sizeof(recv_cc)) {
                recv_cc = ntohl(recv_cc);
                if (recv_cc >> 8 == VANILLA_PIPE_CC_SYNC_STATUS >> 8) {
                    ret = recv_cc & 0xFF;
                    break;
                } else if (recv_cc == VANILLA_PIPE_CC_SYNC_PING) {
                    // Pipe is still responsive but hasn't found anything yet
                    printf("received ping from pipe\n");
                    retries = -1;
                }
            }

            if (is_interrupted()) {
                send_pipe_cc(skt, VANILLA_PIPE_CC_UNBIND, 0);
                break;
            }
        }
    }

    push_event(data->event_loop, VANILLA_EVENT_SYNC, &ret, sizeof(ret));

    // Wait for interrupt so frontend has a chance to receive event
    while (!is_interrupted()) {
        usleep(100000);
    }

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

    int ret = VANILLA_ERR_GENERIC;

    int pipe_cc_skt = 0;
    if (SERVER_ADDRESS != 0) {
        ret = connect_to_backend(&pipe_cc_skt, VANILLA_PIPE_CC_CONNECT);
        if (ret != VANILLA_SUCCESS) {
            goto exit;
        }
    }

    // Open all required sockets
    if (create_socket(&info.socket_vid, PORT_VID) != VANILLA_SUCCESS) goto exit_pipe;
    if (create_socket(&info.socket_msg, PORT_MSG) != VANILLA_SUCCESS) goto exit_vid;
    if (create_socket(&info.socket_hid, PORT_HID) != VANILLA_SUCCESS) goto exit_msg;
    if (create_socket(&info.socket_aud, PORT_AUD) != VANILLA_SUCCESS) goto exit_hid;
    if (create_socket(&info.socket_cmd, PORT_CMD) != VANILLA_SUCCESS) goto exit_aud;

    pthread_t video_thread, audio_thread, input_thread, msg_thread, cmd_thread;

    pthread_create(&video_thread, NULL, listen_video, &info);
    pthread_setname_np(video_thread, "vanilla-video");

    pthread_create(&audio_thread, NULL, listen_audio, &info);
    pthread_setname_np(audio_thread, "vanilla-audio");

    pthread_create(&input_thread, NULL, listen_input, &info);
    pthread_setname_np(input_thread, "vanilla-input");
    
    pthread_create(&cmd_thread, NULL, listen_command, &info);
    pthread_setname_np(cmd_thread, "vanilla-cmd");

    while (1) {
        usleep(250 * 1000);
        if (is_interrupted()) {
            // Wake up any threads that might be blocked on `recv`
            send_stop_code(info.socket_msg, PORT_VID);
            send_stop_code(info.socket_msg, PORT_AUD);
            send_stop_code(info.socket_msg, PORT_CMD);
            break;
        }
    }

    pthread_join(video_thread, NULL);
    pthread_join(audio_thread, NULL);
    pthread_join(input_thread, NULL);
    pthread_join(cmd_thread, NULL);

    if (SERVER_ADDRESS != 0) {
        send_pipe_cc(pipe_cc_skt, VANILLA_PIPE_CC_UNBIND, 0);
    }

    ret = VANILLA_SUCCESS;

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
    if (pipe_cc_skt)
        close(pipe_cc_skt);

exit:
    // TODO: Return error code to frontend?
}

int is_stop_code(const char *data, size_t data_length)
{
    return (data_length == sizeof(STOP_CODE) && !memcmp(data, &STOP_CODE, sizeof(STOP_CODE)));
}

int push_event(event_loop_t *loop, int type, const void *data, size_t size)
{
    pthread_mutex_lock(&loop->mutex);

    vanilla_event_t *ev = &loop->events[loop->new_index % VANILLA_MAX_EVENT_COUNT];

    if (size <= EVENT_BUFFER_SIZE) {
        assert(!ev->data);

        ev->data = get_event_buffer();
        if (!ev->data) {
            print_info("OUT OF MEMORY FOR NEW EVENTS");
            return VANILLA_ERR_OUT_OF_MEMORY;
        }
        
        ev->type = type;
        memcpy(ev->data, data, size);
        ev->size = size;

        loop->new_index++;

        // Prevent rollover by skipping oldest event if necessary
        if (loop->new_index > loop->used_index + VANILLA_MAX_EVENT_COUNT) {
            vanilla_free_event(&loop->events[loop->used_index % VANILLA_MAX_EVENT_COUNT]);
            print_info("SKIPPED EVENT TO PREVENT ROLLOVER (%lu > %lu + %lu)", loop->new_index, loop->used_index, VANILLA_MAX_EVENT_COUNT);
            loop->used_index++;
        }

        pthread_cond_broadcast(&loop->waitcond);
    } else {
        print_info("FAILED TO PUSH EVENT: wanted %lu, only had %lu. This is a bug, please report to developers.\n", size, EVENT_BUFFER_SIZE);
    }

    pthread_mutex_unlock(&loop->mutex);
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
            print_info("CRITICAL: Buffer wasn't returned to the arena");
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
            print_info("CRITICAL: Buffer wasn't returned to the arena");
        }
    }
}