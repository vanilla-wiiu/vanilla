#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <vanilla.h>

#include "pipe.h"

int create_fifo(const char *name)
{
    int f = mkfifo(name, 0777);
    if (f == -1) {
        printf("Failed to create FIFO: %s (%i)\n", name, errno);
    }
    return f;
}

int open_fifo(const char *name, int mode)
{
    int f = open(name, mode);
    if (f == -1) {
        printf("Failed to open FIFO: %s (%i)\n", name, errno);
    }
    return f;
}

int fd_in = 0, fd_out = 0;
uint8_t client_addr[128] = {0};

ssize_t write_pipe(const void *buf, size_t size)
{
    sendto(fd_in, buf, size, 0, (const struct sockaddr *) client_addr, sizeof(client_addr));
}

pthread_mutex_t action_mutex;
int action_ended = 0;
int write_control_code(uint8_t code)
{
    struct pipe_control_code cmd;
    cmd.code = code;
    write_pipe(&cmd, sizeof(cmd));
}

void event_handler(void *context, int event_type, const char *data, size_t data_size)
{
    struct pipe_data_command cmd;

    cmd.base.code = VANILLA_PIPE_OUT_DATA;
    cmd.event_type = event_type;
    cmd.data_size = htons(data_size);
    memcpy(cmd.data, data, data_size);

    write_pipe(&cmd, sizeof(cmd)-sizeof(cmd.data)+data_size);
}

void write_sync_state(uint8_t success)
{
    struct pipe_sync_state_command cmd;
    cmd.base.code = VANILLA_PIPE_OUT_SYNC_STATE;
    cmd.state = success;
    write_pipe(&cmd, sizeof(cmd));
}

struct sync_args
{
    const char *wireless_interface;
    uint16_t code;
};
void *sync_command(void *a)
{
    struct sync_args *args = (struct sync_args *)a;
    int r = vanilla_sync_with_console(args->wireless_interface, args->code);
    free(args);
    write_sync_state(r);

    pthread_mutex_lock(&action_mutex);
    action_ended = 1;
    pthread_mutex_unlock(&action_mutex);

    return (void *) (size_t) r;
}

struct connect_args
{
    const char *wireless_interface;
};
void *connect_command(void *a)
{
    struct connect_args *args = (struct connect_args *) a;
    int r = vanilla_connect_to_console(args->wireless_interface, event_handler, NULL);
    free(args);
    write_control_code(VANILLA_PIPE_OUT_EOF);

    pthread_mutex_lock(&action_mutex);
    action_ended = 1;
    pthread_mutex_unlock(&action_mutex);

    return (void *) (size_t) r;
}

void lib_logger(const char *fmt, va_list args)
{
    vfprintf(stderr, fmt, args);
    // FILE *fff = fopen("/home/matt/Desktop/temp.log", "a");
    // vfprintf(fff, fmt, args);
    // fclose(fff);
}

void vapipelog(const char *str, ...)
{
    va_list va;
    va_start(va, str);

    vfprintf(stderr, str, va);
    fprintf(stderr, "\n");
    
    // FILE *fff = fopen("/home/matt/Desktop/temp.log", "a");
    // vfprintf(fff, str, va);
    // fprintf(fff, "\n");
    // fclose(fff);

    va_end(va);
}

const char *get_positional_arg(int argc, const char **argv, int index)
{
    int cur_index = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            i++;
        } else if (cur_index == index) {
            return argv[i];
        } else {
            cur_index++;
        }
    }
    return NULL;
}

const char *get_optional_arg(int argc, const char **argv, const char *opt_arg_name, int *exists)
{
    *exists = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], opt_arg_name)) {
            *exists = 1;
            if (i + 1 < argc) {
                return argv[i + 1];
            }
            break;
        }
    }

    return NULL;
}

int main(int argc, const char **argv)
{
    if (argc < 3) {
        goto show_help;
    }

    const char *wireless_interface = get_positional_arg(argc, argv, 0);
    if (!wireless_interface) {
        printf("No wireless interface specified\n");
        goto show_help;
    }

    int pipe_exists, udp_exists;
    const char *pipe_mode = get_optional_arg(argc, argv, "-pipe", &pipe_exists);
    const char *udp_mode = get_optional_arg(argc, argv, "-udp", &udp_exists);

    uint8_t address[128] = {0};
    socklen_t address_len;

    if (pipe_exists && udp_exists) {
        printf("-pipe and -udp cannot both be specified\n");
        return 1;
    } else if (pipe_mode) {
        address_len = sizeof(struct sockaddr_un);
    } else if (udp_mode) {
        address_len = sizeof(struct sockaddr_in);
    } else if (pipe_exists) {
        printf("-pipe requires <fifo-path>\n");
        return 1;
    } else if (udp_exists) {
        printf("-udp requires <server-port>\n");
        return 1;
    } else {
        printf("No mode or mode option specified\n");
        goto show_help;
    }

    if (pipe_mode) {
        struct sockaddr_un *sa = (struct sockaddr_un *) address;
        int len = strlen(pipe_mode);
        if (len+1 > sizeof(sa->sun_path)) {
            printf("<fifo-path> is too long, must be less than 108 chars including null-term\n");
            return 1;
        }

        sa->sun_family = AF_UNIX;
        strncpy(sa->sun_path, pipe_mode, sizeof(sa->sun_path)-1);

        fd_in = socket(AF_UNIX, SOCK_STREAM, 0);
    } else {
        uint16_t server_port = atoi(udp_mode);
        if (server_port == 0) {
            printf("UDP port provided was invalid\n");
            return 1;
        }

        struct sockaddr_in *in = (struct sockaddr_in *) address;
        in->sin_family = AF_INET;
        in->sin_addr.s_addr = INADDR_ANY;
        in->sin_port = htons(server_port);

        fd_in = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }

    if (bind(fd_in, (const struct sockaddr *) address, address_len) == -1) {
        printf("Failed to bind to socket\n");
        return 1;
    }

    fprintf(stderr, "READY\n");

    char read_buffer[2048];
    ssize_t read_size;
    pthread_t current_action = 0;
    int m_quit = 0;

    vanilla_install_logger(lib_logger);
    
    pthread_mutex_init(&action_mutex, NULL);

    while (!m_quit) {
        uint8_t sender_addr[sizeof(client_addr)];
        socklen_t sender_addr_len = sizeof(sender_addr);
        read_size = recvfrom(fd_in, read_buffer, sizeof(read_buffer), 0, (struct sockaddr *) sender_addr, &sender_addr_len);
        if (read_size == 0) {
            continue;
        }

        pthread_mutex_lock(&action_mutex);
        if (action_ended) {
            void *ret;
            pthread_join(current_action, &ret);
            action_ended = 0;
            current_action = 0;
        }
        pthread_mutex_unlock(&action_mutex);

        struct pipe_control_code *control_code = (struct pipe_control_code *) read_buffer;
        switch (control_code->code) {
        case VANILLA_PIPE_IN_SYNC:
        {
            if (current_action != 0) {
                write_control_code(VANILLA_PIPE_ERR_BUSY);
            } else {
                struct pipe_sync_command *sync_cmd = (struct pipe_sync_command *) read_buffer;
                sync_cmd->code = ntohs(sync_cmd->code);

                struct sync_args *args = (struct sync_args *) malloc(sizeof(struct sync_args));
                args->code = sync_cmd->code;
                args->wireless_interface = wireless_interface;
                
                write_control_code(VANILLA_PIPE_ERR_SUCCESS);
                
                pthread_create(&current_action, NULL, sync_command, args);
            }
            break;
        }
        case VANILLA_PIPE_IN_CONNECT:
        {
            if (current_action != 0) {
                write_control_code(VANILLA_PIPE_ERR_BUSY);
            } else {
                struct connect_args *args = (struct connect_args *) malloc(sizeof(struct connect_args));
                args->wireless_interface = wireless_interface;
                write_control_code(VANILLA_PIPE_ERR_SUCCESS);
                pthread_create(&current_action, NULL, connect_command, args);
            }
            break;
        }
        case VANILLA_PIPE_IN_BUTTON:
        {
            struct pipe_button_command *btn_cmd = (struct pipe_button_command *) read_buffer;
            btn_cmd->id = ntohl(btn_cmd->id);
            btn_cmd->value = ntohl(btn_cmd->value);
            vanilla_set_button(btn_cmd->id, btn_cmd->value);
            break;
        }
        case VANILLA_PIPE_IN_TOUCH:
        {
            struct pipe_touch_command *touch_cmd = (struct pipe_touch_command *) read_buffer;
            touch_cmd->x = ntohl(touch_cmd->x);
            touch_cmd->y = ntohl(touch_cmd->y);
            vanilla_set_touch(touch_cmd->x, touch_cmd->y);
            break;
        }
        case VANILLA_PIPE_IN_INTERRUPT:
        {
            if (current_action != 0) {
                void *ret;
                vanilla_stop();
            }
            break;
        }
        case VANILLA_PIPE_IN_REQ_IDR:
        {
            vanilla_request_idr();
            break;
        }
        case VANILLA_PIPE_IN_REGION:
        {
            struct pipe_region_command *region_cmd = (struct pipe_region_command *) read_buffer;
            vanilla_set_region(region_cmd->region);
            break;
        }
        case VANILLA_PIPE_IN_BATTERY:
        {
            struct pipe_battery_command *battery_cmd = (struct pipe_battery_command *) read_buffer;
            vanilla_set_battery_status(battery_cmd->battery);
            break;
        }
        case VANILLA_PIPE_IN_QUIT:
            m_quit = 1;
            break;
        case VANILLA_PIPE_IN_BIND:
        {
            struct pipe_command_code *bind_cmd = (struct pipe_command_code *) read_buffer;
            
            // Send success message back to client
            uint8_t cc = VANILLA_PIPE_OUT_BOUND_SUCCESSFUL;
            sendto(fd_in, &cc, sizeof(cc), 0, (const struct sockaddr *) &sender_addr, sizeof(sender_addr));

            // Add client to list
            memcpy(client_addr, sender_addr, sizeof(client_addr));

            struct sockaddr_in *udp_client = (struct sockaddr_in *) client_addr;

            char addr_buf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &udp_client->sin_addr.s_addr, addr_buf, INET_ADDRSTRLEN);
            printf("BIND %s:%u\n", addr_buf, ntohs(udp_client->sin_port));
            break;
        }
        }
    }

    pthread_mutex_destroy(&action_mutex);

    close(fd_in);
    close(fd_out);

    return 0;

show_help:
    printf("Usage: %s <wireless-interface> <mode> [args]\n\n", argv[0]);
    printf("vanilla-pipe provides a way to connect a frontend to Vanilla's backend when they\n");
    printf("aren't able to run on the same device or in the same environment (e.g. when the \n");
    printf("backend must run as root but the frontend must run as user).\n\n");
    printf("Modes:\n\n");
    printf("    -pipe <fifo>\n");
    printf("        Set up communication through Unix FIFO pipes. This is the most reliable\n");
    printf("        solution if you're running the frontend and backend on the same device.\n\n");
    printf("    -udp <port>\n\n");
    printf("        Set up communication through networked UDP port. This is the best option\n");
    printf("        if the frontend and backend are on separate devices.\n\n");
    return 1;
}