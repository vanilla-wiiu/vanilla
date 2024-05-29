#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
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

pthread_mutex_t output_mutex;
pthread_mutex_t action_mutex;
int action_ended = 0;
int fd_out = 0;
int write_control_code(uint8_t code)
{
    pthread_mutex_lock(&output_mutex);
    write(fd_out, &code, 1);
    pthread_mutex_unlock(&output_mutex);
}

void event_handler(void *context, int event_type, const char *data, size_t data_size)
{
    uint8_t event_sized = event_type;
    uint64_t data_size_sized = data_size;
    uint8_t control_code = VANILLA_PIPE_OUT_DATA;

    pthread_mutex_lock(&output_mutex);

    write(fd_out, &control_code, sizeof(control_code));
    write(fd_out, &event_sized, sizeof(event_sized));
    write(fd_out, &data_size_sized, sizeof(data_size_sized));
    write(fd_out, data, data_size);

    pthread_mutex_unlock(&output_mutex);
}

void write_sync_state(uint8_t success)
{
    uint8_t cc = VANILLA_PIPE_OUT_SYNC_STATE;

    pthread_mutex_lock(&output_mutex);

    write(fd_out, &cc, sizeof(cc));
    write(fd_out, &success, sizeof(success));

    pthread_mutex_unlock(&output_mutex);
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

    pthread_exit((void *) (size_t) r);
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

    pthread_exit((void *) (size_t) r);
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

int main()
{
    int fd_in;

    time_t now = time(0);
    char pipe_in_filename[256];
    char pipe_out_filename[256];
    snprintf(pipe_in_filename, sizeof(pipe_in_filename), "%s-%li", "/tmp/vanilla-fifo-in", now);
    snprintf(pipe_out_filename, sizeof(pipe_out_filename), "%s-%li", "/tmp/vanilla-fifo-out", now);

    umask(0000);

    if (create_fifo(pipe_in_filename) == -1) return 1;
    if (create_fifo(pipe_out_filename) == -1) return 1;

    fprintf(stderr, "%s\n", pipe_in_filename);
    fprintf(stderr, "%s\n", pipe_out_filename);

    if ((fd_out = open_fifo(pipe_out_filename, O_WRONLY)) == -1) return 1;
    if ((fd_in = open_fifo(pipe_in_filename, O_RDONLY)) == -1) return 1;

    uint8_t control_code;
    ssize_t read_size;
    pthread_t current_action = 0;
    int m_quit = 0;

    vanilla_install_logger(lib_logger);
    
    pthread_mutex_init(&output_mutex, NULL);
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
            int16_t button_value;
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
        case VANILLA_PIPE_IN_QUIT:
            m_quit = 1;
            break;
        }
    }

    pthread_mutex_destroy(&action_mutex);
    pthread_mutex_destroy(&output_mutex);

    close(fd_in);
    close(fd_out);

    unlink(pipe_in_filename);
    unlink(pipe_out_filename);

    return 0;
}