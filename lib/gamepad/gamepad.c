#include "gamepad.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wpa_ctrl.h>

#include "audio.h"
#include "command.h"
#include "input.h"
#include "video.h"

#include "status.h"
#include "util.h"
#include "wpa.h"

static const uint32_t STOP_CODE = 0xCAFEBABE;

unsigned int reverse_bits(unsigned int b, int bit_count)
{
    unsigned int result = 0;

    for (int i = 0; i < bit_count; i++) {
        result |= ((b >> i) & 1) << (bit_count - 1 -i );
    }

    return result;
}

void send_to_console(int fd, const void *data, size_t data_size, int port)
{
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("192.168.1.10");
    address.sin_port = htons((uint16_t) (port - 100));
    ssize_t sent = sendto(fd, data, data_size, 0, (const struct sockaddr *) &address, sizeof(address));
    if (sent == -1) {
        print_info("Failed to send to Wii U socket: fd - %d; port - %d", fd, port);
    }
}

int create_socket(int *socket_out, uint16_t port)
{
    // TODO: Limit these sockets to one interface?

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    (*socket_out) = socket(AF_INET, SOCK_DGRAM, 0);
    
    //setsockopt((*socket_out), SOL_SOCKET, SO_RCVTIMEO)
    
    if (bind((*socket_out), (const struct sockaddr *) &address, sizeof(address)) == -1) {
        print_info("FAILED TO BIND PORT %u: %i", port, errno);
        return 0;
    }

    return 1;
}

void send_stop_code(int from_socket, in_port_t port)
{
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");

    address.sin_port = htons(port);
    sendto(from_socket, &STOP_CODE, sizeof(STOP_CODE), 0, (struct sockaddr *)&address, sizeof(address));
}

int main_loop(vanilla_event_handler_t event_handler, void *context)
{
    struct gamepad_thread_context info;
    info.event_handler = event_handler;
    info.context = context;

    int ret = VANILLA_ERROR;

    // Open all required sockets
    if (!create_socket(&info.socket_vid, PORT_VID)) goto exit;
    if (!create_socket(&info.socket_msg, PORT_MSG)) goto exit_vid;
    if (!create_socket(&info.socket_hid, PORT_HID)) goto exit_msg;
    if (!create_socket(&info.socket_aud, PORT_AUD)) goto exit_hid;
    if (!create_socket(&info.socket_cmd, PORT_CMD)) goto exit_aud;

    pthread_t video_thread, audio_thread, input_thread, msg_thread, cmd_thread;

    pthread_create(&video_thread, NULL, listen_video, &info);
    pthread_create(&audio_thread, NULL, listen_audio, &info);
    pthread_create(&input_thread, NULL, listen_input, &info);
    pthread_create(&cmd_thread, NULL, listen_command, &info);

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

exit:
    return ret;
}

int connect_as_gamepad_internal(struct wpa_ctrl *ctrl, const char *wireless_interface, vanilla_event_handler_t event_handler, void *context)
{
    while (1) {
        while (!wpa_ctrl_pending(ctrl)) {
            sleep(2);
            print_info("WAITING FOR CONNECTION");

            if (is_interrupted()) return VANILLA_ERROR;
        }

        char buf[1024];
        size_t actual_buf_len = sizeof(buf);
        wpa_ctrl_recv(ctrl, buf, &actual_buf_len);
        print_info("CONN RECV: %.*s", actual_buf_len, buf);

        if (memcmp(buf, "<3>CTRL-EVENT-CONNECTED", 23) == 0) {
            break;
        }

        if (is_interrupted()) return VANILLA_ERROR;
    }

    print_info("CONNECTED TO CONSOLE");

    // Use DHCP on interface
    pid_t dhclient_pid;
    int r = call_dhcp(wireless_interface, &dhclient_pid);
    if (r != VANILLA_SUCCESS) {
        print_info("FAILED TO RUN DHCP ON %s", wireless_interface);
        return r;
    } else {
        print_info("DHCP ESTABLISHED");
    }

    {
        // Destroy default route that dhclient will have created
        pid_t ip_pid;
        const char *ip_args[] = {"ip", "route", "del", "default", "via", "192.168.1.1", "dev", wireless_interface, NULL};
        r = start_process(ip_args, &ip_pid, NULL, NULL);
        if (r != VANILLA_SUCCESS) {
            print_info("FAILED TO REMOVE CONSOLE ROUTE FROM SYSTEM");
        }

        int ip_status;
        waitpid(ip_pid, &ip_status, 0);

        if (!WIFEXITED(ip_status)) {
            print_info("FAILED TO REMOVE CONSOLE ROUTE FROM SYSTEM");
        }
    }

    r = main_loop(event_handler, context);

    int kill_ret = kill(dhclient_pid, SIGTERM);
    print_info("killing dhclient %i: %i", dhclient_pid, kill_ret);

    return r;
}

int is_stop_code(const char *data, size_t data_length)
{
    return (data_length == sizeof(STOP_CODE) && !memcmp(data, &STOP_CODE, sizeof(STOP_CODE)));
}