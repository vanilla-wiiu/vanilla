#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../lib/gamepad/ports.h"

typedef struct {
    int from;
    int to;
} relay_ports;

struct in_addr client_address = {0};

void* do_relay(void *data)
{
    relay_ports *ports = (relay_ports *) data;
    char buf[2048];
    ssize_t read_size;
    while ((read_size = recv(ports->from, buf, sizeof(buf), 0)) != -1) {
        struct sockaddr_in forward = {0};
        forward.sin_family = AF_INET;
        forward.sin_addr = client_address;
        forward.sin_port = ports->to;
        sendto(ports->to, buf, read_size, 0, (const struct sockaddr *) &forward, sizeof(forward));
    }
    return NULL;
}

int open_socket(in_port_t port)
{
    struct sockaddr_in in = {0};
    in.sin_family = AF_INET;
    in.sin_addr.s_addr = INADDR_ANY;
    in.sin_port = htons(port);
    
    int skt = socket(AF_INET, SOCK_DGRAM, 0);
    if (skt == -1) {
        return -1;
    }
    
    if (bind(skt, (const struct sockaddr *) &in, sizeof(in)) == -1) {
        printf("FAILED TO BIND PORT %u: %i\n", port, errno);
        close(skt);
        return -1;
    }

    return skt;
}

void *open_relay(void *data)
{
    in_port_t port = (in_port_t) data;
    int ret = -1;

    // Open an incoming port from the console
    int from_console = open_socket(port);
    if (from_console == -1) {
        goto close;
    }

    // Open an incoming port from the frontend
    int from_frontend = open_socket(port + 100);
    if (from_frontend == -1) {
        goto close_console_connection;
    }

    relay_ports a_ports, b_ports;

    a_ports.from = from_console;
    a_ports.to = from_frontend;

    b_ports.from = from_frontend;
    b_ports.to = from_console;

    pthread_t a_thread, b_thread;
    pthread_create(&a_thread, NULL, do_relay, &a_ports);
    pthread_create(&b_thread, NULL, do_relay, &b_ports);

    pthread_join(a_thread, NULL);
    pthread_join(b_thread, NULL);

    ret = 0;

close_frontend_connection:
    close(from_frontend);

close_console_connection:
    close(from_console);

close:
    return (void *) ret;
}

int main(int argc, const char **argv)
{
    if (argc != 3) {
        goto show_help;
    }

    if (!strcmp("-sync", argv[1])) {
        int code = atoi(argv[2]);
        if (code == 0) {
            printf("ERROR: Invalid sync code\n\n");
            goto show_help;
        }


    } else if (!strcmp("-connect", argv[1])) {
        inet_pton(AF_INET, argv[2], &client_address);

        pthread_t vid_thread, aud_thread, msg_thread, cmd_thread, hid_thread;

        pthread_create(&vid_thread, NULL, open_relay, (void *) PORT_VID);
        pthread_create(&aud_thread, NULL, open_relay, (void *) PORT_AUD);
        pthread_create(&msg_thread, NULL, open_relay, (void *) PORT_MSG);
        pthread_create(&cmd_thread, NULL, open_relay, (void *) PORT_CMD);
        pthread_create(&hid_thread, NULL, open_relay, (void *) PORT_HID);

        printf("READY\n");

        pthread_join(vid_thread, NULL);
        pthread_join(aud_thread, NULL);
        pthread_join(msg_thread, NULL);
        pthread_join(cmd_thread, NULL);
        pthread_join(hid_thread, NULL);
    } else {
        printf("ERROR: Invalid mode\n\n");
        goto show_help;
    }

    return 0;

show_help:
    printf("vanilla-pipe - brokers a connection between Vanilla and the Wii U\n");
    printf("\n");
    printf("Usage: %s <mode> <args>\n", argv[0]);
    printf("\n");
    printf("Modes: \n");
    printf("  -sync <code>                 Sync/authenticate with the Wii U\n");
    printf("  -connect <client-address>    Connect to the Wii U (requires syncing prior)\n");
    printf("\n");

    return 1;
}