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

uint8_t buffer[1048576];
size_t buffer_pos = 0;
pthread_mutex_t buffer_mutex;
int fd_in = 0, fd_out = 0;
in_addr_t udp_client_addr = 0;
in_port_t udp_client_port = 0;

void buffer_start()
{
    pthread_mutex_lock(&buffer_mutex);
}

void buffer_write(const void *data, size_t length)
{
    memcpy(buffer + buffer_pos, data, length);
    buffer_pos += length;
}

void buffer_finish()
{
    if (fd_out != 0) {
        write(fd_out, buffer, buffer_pos);
    }
    
    if (udp_client_addr != 0 && udp_client_port != 0) {
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = udp_client_addr;
        address.sin_port = htons(udp_client_port);
        sendto(fd_in, buffer, buffer_pos, 0, (const struct sockaddr *) &address, sizeof(address));
    }

    buffer_pos = 0;
    pthread_mutex_unlock(&buffer_mutex);
}

void buffer_write_single(void *data, size_t length)
{
    buffer_start();
    buffer_write(data, length);
    buffer_finish();
}

pthread_mutex_t action_mutex;
int action_ended = 0;
int write_control_code(uint8_t code)
{
    buffer_write_single(&code, 1);
}

void event_handler(void *context, int event_type, const char *data, size_t data_size)
{
    uint8_t event_sized = event_type;
    uint64_t data_size_sized = data_size;
    uint8_t control_code = VANILLA_PIPE_OUT_DATA;

    buffer_start();
    buffer_write(&control_code, sizeof(control_code));
    buffer_write(&event_sized, sizeof(event_sized));
    buffer_write(&data_size_sized, sizeof(data_size_sized));
    buffer_write(data, data_size);
    buffer_finish();
}

void write_sync_state(uint8_t success)
{
    uint8_t cc = VANILLA_PIPE_OUT_SYNC_STATE;

    buffer_start();
    buffer_write(&cc, sizeof(cc));
    buffer_write(&success, sizeof(success));
    buffer_finish();
}

#define WIRELESS_INTERFACE_MAX_LEN 100
struct sync_args
{
    char wireless_interface[WIRELESS_INTERFACE_MAX_LEN];
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
    char wireless_interface[WIRELESS_INTERFACE_MAX_LEN];
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

void read_string(int fd, char *buf, size_t max)
{
    for (size_t i = 0; i < max; i++) {
        read(fd, &buf[i], 1);
        if (buf[i] == 0) {
            break;
        }
    }
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

int main(int argc, char **argv)
{
    if (argc < 2) {
        goto show_help;
    }

    if (!strcmp(argv[1], "-pipe")) {
        if (argc < 4) {
            printf("-pipe requires <in-fifo> and <out-fifo>\n");
            goto show_help;
        }

        const char *pipe_in_filename = argv[2];
        const char *pipe_out_filename = argv[3];

        umask(0000);

        if (create_fifo(pipe_in_filename) == -1) return 1;
        if (create_fifo(pipe_out_filename) == -1) return 1;

        fprintf(stderr, "READY\n");

        if ((fd_out = open_fifo(pipe_out_filename, O_WRONLY)) == -1) return 1;
        if ((fd_in = open_fifo(pipe_in_filename, O_RDONLY)) == -1) return 1;
    } else if (!strcmp(argv[1], "-udp")) {
        if (argc < 5) {
            printf("-udp requires <server-port> <client-address> <client-port>\n");
            goto show_help;
        }

        uint16_t server_port = atoi(argv[2]);
        if (server_port == 0) {
            printf("UDP port provided was invalid\n");
            goto show_help;
        }

        udp_client_addr = inet_addr(argv[3]);
        if (udp_client_addr == 0) {
            printf("Client IP provided was invalid\n");
            goto show_help;
        }

        udp_client_port = atoi(argv[4]);
        if (udp_client_port == 0) {
            printf("Client port provided was invalid\n");
            goto show_help;
        }

        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(server_port);
        fd_in = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        
        if (bind(fd_in, (const struct sockaddr *) &address, sizeof(address)) == -1) {
            printf("Failed to bind to port %u\n", server_port);
            return 1;
        }

        fprintf(stderr, "READY\n");
    } else {
        printf("Unknown mode '%s'\n", argv[1]);
        goto show_help;
    }

    uint8_t control_code;
    ssize_t read_size;
    pthread_t current_action = 0;
    int m_quit = 0;

    vanilla_install_logger(lib_logger);
    
    pthread_mutex_init(&buffer_mutex, NULL);
    pthread_mutex_init(&action_mutex, NULL);

    while (!m_quit) {
        read_size = read(fd_in, &control_code, 1);
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

        switch (control_code) {
        case VANILLA_PIPE_IN_SYNC:
        {
            if (current_action != 0) {
                write_control_code(VANILLA_PIPE_ERR_BUSY);
            } else {
                struct sync_args *args = (struct sync_args *) malloc(sizeof(struct sync_args));
                read(fd_in, &args->code, sizeof(args->code));
                read_string(fd_in, args->wireless_interface, sizeof(args->wireless_interface));
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
                read_string(fd_in, args->wireless_interface, sizeof(args->wireless_interface));
                write_control_code(VANILLA_PIPE_ERR_SUCCESS);
                pthread_create(&current_action, NULL, connect_command, args);
            }
            break;
        }
        case VANILLA_PIPE_IN_BUTTON:
        {
            int32_t button_id;
            int32_t button_value;
            read(fd_in, &button_id, sizeof(button_id));
            read(fd_in, &button_value, sizeof(button_value));
            vanilla_set_button(button_id, button_value);
            break;
        }
        case VANILLA_PIPE_IN_TOUCH:
        {
            int32_t touch_x;
            int32_t touch_y;
            read(fd_in, &touch_x, sizeof(touch_x));
            read(fd_in, &touch_y, sizeof(touch_y));
            vanilla_set_touch(touch_x, touch_y);
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
            int8_t region;
            read(fd_in, &region, sizeof(region));
            vanilla_set_region(region);
            break;
        }
        case VANILLA_PIPE_IN_BATTERY:
        {
            int8_t battery;
            read(fd_in, &battery, sizeof(battery));
            vanilla_set_battery_status(battery);
            break;
        }
        case VANILLA_PIPE_IN_QUIT:
            m_quit = 1;
            break;
        }
    }

    pthread_mutex_destroy(&action_mutex);
    pthread_mutex_destroy(&buffer_mutex);

    close(fd_in);
    close(fd_out);

    return 0;

show_help:
    printf("Usage: %s <mode> [args]\n\n", argv[0]);
    printf("vanilla-pipe provides a way to connect a frontend to Vanilla's backend when they\n");
    printf("aren't able to run on the same device or in the same environment (e.g. when the \n");
    printf("backend must run as root but the frontend must run as user).\n\n");
    printf("Modes:\n\n");
    printf("    -pipe <in-fifo> <out-fifo>\n");
    printf("        Set up communication through Unix FIFO pipes. This is the most reliable\n");
    printf("        solution if you're running the frontend and backend on the same device.\n\n");
    printf("    -udp <port>\n\n");
    printf("        Set up communication through networked UDP port. This is the best option\n");
    printf("        if the frontend and backend are on separate devices.\n\n");
    return 1;
}