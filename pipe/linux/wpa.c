#include "wpa.h"

#include <arpa/inet.h>
#include <errno.h>
#include <libgen.h>
#include <netlink/addr.h>
#include <netlink/netlink.h>
#include <netlink/route/addr.h>
#include <netlink/route/link.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wpa_ctrl.h>

#include "../def.h"
#include "ports.h"
#include "status.h"
#include "util.h"
#include "vanilla.h"
#include "wpa.h"

#include <utils/common.h>
#include <wpa_supplicant_i.h>
#include <libnm/NetworkManager.h>

extern int udhcpc_main(const char **argv);

const char *wpa_ctrl_interface = "/var/run/wpa_supplicant_drc";

pthread_mutex_t running_mutex;
pthread_mutex_t main_loop_mutex;
pthread_mutex_t action_mutex;
pthread_mutex_t sync_mutex;
int running = 0;
int main_loop = 0;
int sync_result_ready = 0;
uint8_t sync_result = 0;

typedef struct {
    int from_socket;
    in_port_t from_port;
    int to_socket;
    in_addr_t to_address;
    in_port_t to_port;
} relay_ports;

struct in_addr client_address = {0};

struct sync_args {
    const char *wireless_interface;
    const char *wireless_config;
    uint16_t code;
    void *(*start_routine)(void *);
    struct wpa_ctrl *ctrl;
};

struct relay_info {
    const char *wireless_interface;
    in_port_t port;
};

#define THREADRESULT(x) ((void *) (uintptr_t) (x))

void lpprint(const char *fmt, va_list args)
{
    vfprintf(stderr, fmt, args);
}

void pprint(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    lpprint(fmt, args);
    va_end(args);
}

void print_info(const char *errstr, ...)
{
    va_list args;
    va_start(args, errstr);

    lpprint(errstr, args);
    pprint("\n");

    va_end(args);
}

int is_interrupted()
{
    pthread_mutex_lock(&running_mutex);
    int r = !running;
    pthread_mutex_unlock(&running_mutex);
    return r;
}

void vanilla_wpa_msg(char *msg, size_t len)
{
    print_info("%.*s", len, msg);
}

void wpa_ctrl_command(struct wpa_ctrl *ctrl, const char *cmd, char *buf, size_t *buf_len)
{
    wpa_ctrl_request(ctrl, cmd, strlen(cmd), buf, buf_len, NULL /*wpa_msg*/);
}

void quit_loop()
{
    pthread_mutex_lock(&running_mutex);
    pthread_mutex_lock(&main_loop_mutex);
    running = 0;
    main_loop = 0;
    pthread_mutex_unlock(&main_loop_mutex);
    pthread_mutex_unlock(&running_mutex);
}

void sigint_handler(int signum)
{
    if (signum == SIGINT) {
        pprint("RECEIVED INTERRUPT SIGNAL\n");
    } else if (signum == SIGTERM) {
        pprint("RECEIVED TERMINATE SIGNAL\n");
    }
    quit_loop();
    signal(signum, SIG_DFL);
}

void *read_stdin(void *)
{
    char *line = NULL;
    size_t size = 0;
    ssize_t read_size = 0;

    while ((read_size = getline(&line, &size, stdin)) != -1) {
        if (read_size == 0) {
            continue;
        }

        line[read_size-1] = '\0';
        if (!strcasecmp(line, "quit") || !strcasecmp(line, "exit") || !strcasecmp(line, "bye")) {
            quit_loop();
        }
    }

    free(line);
    return NULL;
}

void *start_wpa(void *arg)
{
    return THREADRESULT(wpa_supplicant_run((struct wpa_global *) arg));
}

void *wpa_setup_environment(void *data)
{
    void *ret = THREADRESULT(VANILLA_ERROR);

    struct sync_args *args = (struct sync_args *) data;

    // Check status of interface with NetworkManager
    GError *nm_err;
    NMClient *nmcli = nm_client_new(NULL, &nm_err);
    NMDevice *nmdev = NULL;
    gboolean is_managed = 0;
    if (nmcli) {
        nmdev = nm_client_get_device_by_iface(nmcli, args->wireless_interface);
        if (!nmdev) {
            pprint("FAILED TO GET STATUS OF DEVICE %s\n", args->wireless_interface);
            goto die_and_close_nmcli;
        }

        if ((is_managed = nm_device_get_managed(nmdev))) {
            nm_device_set_managed(nmdev, FALSE);
            print_info("TEMPORARILY SET %s TO UNMANAGED", args->wireless_interface);
        }
    } else {
        // Failed to get NetworkManager, host may just not have it?
        g_message("Failed to create NetworkManager client: %s", nm_err->message);
        g_error_free(nm_err);
    }

    // Start modified WPA supplicant
    struct wpa_params params = {0};
    params.wpa_debug_level = MSG_INFO;

    struct wpa_interface interface = {0};
    interface.driver = "nl80211";
    interface.ifname = args->wireless_interface;
    interface.confname = args->wireless_config;
    
    struct wpa_global *wpa = wpa_supplicant_init(&params);
    if (!wpa) {
        print_info("FAILED TO INIT WPA SUPPLICANT");
        goto die_and_reenable_managed;
    }

    struct wpa_supplicant *wpa_s = wpa_supplicant_add_iface(wpa, &interface, NULL);
    if (!wpa_s) {
        print_info("FAILED TO ADD WPA IFACE");
        goto die_and_kill;
    }

    pthread_t wpa_thread;
    pthread_create(&wpa_thread, NULL, start_wpa, wpa);

    // Get control interface
    const size_t buf_len = 1048576;
    char *buf = malloc(buf_len);
    snprintf(buf, buf_len, "%s/%s", wpa_ctrl_interface, args->wireless_interface);
    struct wpa_ctrl *ctrl;
    while (!(ctrl = wpa_ctrl_open(buf))) {
        if (is_interrupted()) goto die_and_free_buf;
        print_info("WAITING FOR CTRL INTERFACE");
        sleep(1);
    }

    if (is_interrupted() || wpa_ctrl_attach(ctrl) < 0) {
        print_info("FAILED TO ATTACH TO WPA");
        goto die_and_close;
    }

    args->ctrl = ctrl;
    ret = args->start_routine(args);

die_and_detach:
    wpa_ctrl_detach(ctrl);

die_and_close:
    wpa_ctrl_close(ctrl);

die_and_free_buf:
    free(buf);

die_and_kill:
    pthread_kill(wpa_thread, SIGINT);
    pthread_join(wpa_thread, NULL);
    wpa_supplicant_deinit(wpa);

die_and_reenable_managed:
    if (is_managed) {
        print_info("SETTING %s BACK TO MANAGED", args->wireless_interface);
        nm_device_set_managed(nmdev, TRUE);
    }

die_and_close_nmcli:
    if (nmcli) {
        g_object_unref(nmcli);
    }

die:
    return ret;
}

void *do_relay(void *data)
{
    relay_ports *ports = (relay_ports *) data;
    char buf[2048];
    ssize_t read_size;
    while (!is_interrupted() && client_address.s_addr != 0) {
        read_size = recv(ports->from_socket, buf, sizeof(buf), 0);
        if (read_size <= 0) {
            continue;
        }

        struct sockaddr_in forward = {0};
        forward.sin_family = AF_INET;
        forward.sin_addr.s_addr = ports->to_address;
        forward.sin_port = htons(ports->to_port);

        char ip[20];
        inet_ntop(AF_INET, &forward.sin_addr, ip, sizeof(ip));
        // printf("received packet on port %i, forwarding to address %s:%i\n", ports->from_port, ip, ports->to_port);
        sendto(ports->to_socket, buf, read_size, 0, (const struct sockaddr *) &forward, sizeof(forward));
    }
    return NULL;
}

relay_ports create_ports(int from_socket, in_port_t from_port, int to_socket, in_addr_t to_addr, in_port_t to_port)
{
    relay_ports ports;
    ports.from_socket = from_socket;
    ports.from_port = from_port;
    ports.to_socket = to_socket;
    ports.to_address = to_addr;
    ports.to_port = to_port;
    return ports;
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

    struct timeval tv = {0};
    tv.tv_usec = 250000;
    setsockopt(skt, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    if (bind(skt, (const struct sockaddr *) &in, sizeof(in)) == -1) {
        print_info("FAILED TO BIND PORT %u: %i\n", port, errno);
        close(skt);
        return -1;
    }

    return skt;
}

void *open_relay(void *data)
{
    struct relay_info *info = (struct relay_info *) data;
    in_port_t port = info->port;
    int ret = -1;

    // Open an incoming port from the console
    int from_console = open_socket(port);
    if (from_console == -1) {
        goto close;
    }

    setsockopt(from_console, SOL_SOCKET, SO_BINDTODEVICE, info->wireless_interface, strlen(info->wireless_interface));

    // Open an incoming port from the frontend
    int from_frontend = open_socket(port + 100);
    if (from_frontend == -1) {
        goto close_console_connection;
    }

    // print_info("ENTERING MAIN LOOP");
    while (!is_interrupted()) {
        if (client_address.s_addr != 0) {
            print_info("STARTED RELAYS");
            relay_ports console_to_frontend = create_ports(from_console, port, from_frontend, client_address.s_addr, port + 200);
            relay_ports frontend_to_console = create_ports(from_frontend, port + 100, from_console, inet_addr("192.168.1.10"), port - 100);

            pthread_t a_thread, b_thread;
            pthread_create(&a_thread, NULL, do_relay, &console_to_frontend);
            pthread_create(&b_thread, NULL, do_relay, &frontend_to_console);

            pthread_join(a_thread, NULL);
            pthread_join(b_thread, NULL);

            print_info("STOPPED RELAYS");
        }

        // print_info("RELAY EXITED");
    }

    ret = 0;

close_frontend_connection:
    close(from_frontend);

close_console_connection:
    close(from_console);

close:
    return THREADRESULT(ret);
}

void create_all_relays(const char *wireless_interface)
{
    pthread_t vid_thread, aud_thread, msg_thread, cmd_thread, hid_thread;
    struct relay_info vid_info, aud_info, msg_info, cmd_info, hid_info;

    // Set wireless interface on all
    vid_info.wireless_interface = aud_info.wireless_interface = msg_info.wireless_interface = cmd_info.wireless_interface = hid_info.wireless_interface = wireless_interface;
    
    vid_info.port = PORT_VID;
    aud_info.port = PORT_AUD;
    msg_info.port = PORT_MSG;
    cmd_info.port = PORT_CMD;
    hid_info.port = PORT_HID;

    pthread_create(&vid_thread, NULL, open_relay, &vid_info);
    pthread_create(&aud_thread, NULL, open_relay, &aud_info);
    pthread_create(&msg_thread, NULL, open_relay, &msg_info);
    pthread_create(&cmd_thread, NULL, open_relay, &cmd_info);
    pthread_create(&hid_thread, NULL, open_relay, &hid_info);

    pthread_join(vid_thread, NULL);
    pthread_join(aud_thread, NULL);
    pthread_join(msg_thread, NULL);
    pthread_join(cmd_thread, NULL);
    pthread_join(hid_thread, NULL);
}

void *thread_handler(void *data)
{
    struct sync_args *args = (struct sync_args *) data;

    pthread_mutex_lock(&running_mutex);
    running = 1;
    pthread_mutex_unlock(&running_mutex);

    void *ret = args->start_routine(data);
    
    free(args);

    pthread_mutex_lock(&running_mutex);
    running = 0;
    pthread_mutex_unlock(&running_mutex);

    // Locked by calling thread
    pthread_mutex_unlock(&action_mutex);
    
    return ret;
}

char wireless_authenticate_config_filename[1024] = {0};
char wireless_connect_config_filename[1024] = {0};

size_t get_home_directory(char *buf, size_t buf_size)
{
    size_t ret = snprintf(buf, buf_size, "%s/%s", getenv("HOME"), ".vanilla");
    if (ret <= buf_size) {
        mkdir(buf, 0755);
    }
    return ret;
}

size_t get_home_directory_file(const char *filename, char *buf, size_t buf_size)
{
    size_t max_path_length = get_max_path_length();
    char *dir = malloc(max_path_length);
    get_home_directory(dir, max_path_length);

    size_t ret = snprintf(buf, buf_size, "%s/%s", dir, filename);
    free(dir);
    return ret;
}

size_t get_max_path_length()
{
    return pathconf(".", _PC_PATH_MAX);
}

const char *get_wireless_connect_config_filename()
{
    if (wireless_connect_config_filename[0] == 0) {
        // Not initialized yet, do this now
        get_home_directory_file("vanilla_wpa_connect.conf", wireless_connect_config_filename, sizeof(wireless_connect_config_filename));
    }
    return wireless_connect_config_filename;
}

const char *get_wireless_authenticate_config_filename()
{
    if (wireless_authenticate_config_filename[0] == 0) {
        // Not initialized yet, do this now
        get_home_directory_file("vanilla_wpa_key.conf", wireless_authenticate_config_filename, sizeof(wireless_authenticate_config_filename));
    }
    return wireless_authenticate_config_filename;
}

size_t read_line_from_fd(int pipe, char *output, size_t max_output_size)
{
    size_t i = 0;
    while (i < max_output_size && read(pipe, output, 1) > 0) {
        int newline = (*output == '\n');
        output++;
        i++;
        if (newline) {
            break;
        }
    }
    *output = 0;
    return i;
}

size_t read_line_from_file(FILE *file, char *output, size_t max_output_size)
{
    return read_line_from_fd(fileno(file), output, max_output_size);
}

int create_connect_config(const char *input_config, const char *bssid)
{
    FILE *in_file = fopen(input_config, "r");
    if (!in_file) {
        print_info("FAILED TO OPEN INPUT CONFIG FILE");
        return VANILLA_ERROR;
    }
    
    FILE *out_file = fopen(get_wireless_connect_config_filename(), "w");
    if (!out_file) {
        print_info("FAILED TO OPEN OUTPUT CONFIG FILE");
        fclose(in_file);
        return VANILLA_ERROR;
    }

    int len;
    char buf[150];
    while (len = read_line_from_file(in_file, buf, sizeof(buf))) {
        if (memcmp("\tssid=", buf, 6) == 0) {
            fprintf(out_file, "\tscan_ssid=1\n\tbssid=%s\n", bssid);
        }

        fwrite(buf, len, 1, out_file);

        if (memcmp(buf, "update_config=1", 15) == 0) {
            static const char *ap_scan_line = "ap_scan=1\n";
            fwrite(ap_scan_line, strlen(ap_scan_line), 1, out_file);
        }
    }

    fclose(in_file);
    fclose(out_file);

    return VANILLA_SUCCESS;
}

void *sync_with_console_internal(void *data)
{
    struct sync_args *args = (struct sync_args *) data;

    char buf[16384];
    const size_t buf_len = sizeof(buf);

    int found_console = 0;
    char bssid[18];
    do {
        size_t actual_buf_len;

        if (is_interrupted()) goto exit_loop;

        // Request scan from hardware
        while (1) {
            if (is_interrupted()) goto exit_loop;

            // print_info("SCANNING");
            actual_buf_len = buf_len;
            wpa_ctrl_command(args->ctrl, "SCAN", buf, &actual_buf_len);

            if (!memcmp(buf, "FAIL-BUSY", 9)) {
                //print_info("DEVICE BUSY, RETRYING");
                sleep(5);
            } else if (!memcmp(buf, "OK", 2)) {
                break;
            } else {
                print_info("UNKNOWN SCAN RESPONSE: %.*s (RETRYING)", actual_buf_len, buf);
                sleep(5);
            }
        }

        //print_info("WAITING FOR SCAN RESULTS");
        actual_buf_len = buf_len;
        wpa_ctrl_command(args->ctrl, "SCAN_RESULTS", buf, &actual_buf_len);
        print_info("RECEIVED SCAN RESULTS");

        const char *line = strtok(buf, "\n");
        while (line) {
            if (is_interrupted()) goto exit_loop;

            if (strstr(line, "WiiU")) {
                print_info("FOUND WII U, TESTING WPS PIN");

                // Make copy of bssid for later
                strncpy(bssid, line, sizeof(bssid));
                bssid[17] = '\0';

                char wps_buf[100];
                snprintf(wps_buf, sizeof(wps_buf), "WPS_PIN %.*s %04d5678", 17, bssid, args->code);

                size_t actual_buf_len = buf_len;
                wpa_ctrl_command(args->ctrl, wps_buf, buf, &actual_buf_len);

                static const int max_wait = 20;
                int wait_count = 0;
                int cred_received = 0;

                while (!is_interrupted()) {
                    while (wait_count < max_wait && !wpa_ctrl_pending(args->ctrl)) {
                        if (is_interrupted()) goto exit_loop;
                        sleep(1);
                        wait_count++;
                    }

                    if (wait_count == max_wait) {
                        print_info("GIVING UP, RETURNING TO SCANNING");
                        break;
                    }

                    actual_buf_len = buf_len;
                    wpa_ctrl_recv(args->ctrl, buf, &actual_buf_len);
                    if (!strstr(buf, "CTRL-EVENT-BSS-ADDED")
                        && !strstr(buf, "CTRL-EVENT-BSS-REMOVED")) {
                        print_info("CRED RECV: %.*s", buf_len, buf);
                    }

                    if (!memcmp("<3>WPS-CRED-RECEIVED", buf, 20)) {
                        print_info("RECEIVED AUTHENTICATION FROM CONSOLE");
                        cred_received = 1;
                        break;
                    }
                }

                if (cred_received) {
                    // Tell wpa_supplicant to save config
                    actual_buf_len = buf_len;
                    print_info("SAVING CONFIG", actual_buf_len, buf);
                    wpa_ctrl_command(args->ctrl, "SAVE_CONFIG", buf, &actual_buf_len);

                    // Create connect config which needs a couple more parameters
                    create_connect_config(get_wireless_authenticate_config_filename(), bssid);

                    found_console = 1;
                }
            }
            line = strtok(NULL, "\n");
        }
    } while (!found_console);

exit_loop:
    return THREADRESULT(found_console ? VANILLA_SUCCESS : VANILLA_ERROR);
}

int apply_addr_on_intf(const char *wireless_interface, const char *addr_to_set)
{
    int ret = VANILLA_ERROR;

    struct nl_sock* sk = nl_socket_alloc();

    if (nl_connect(sk, NETLINK_ROUTE)) {
        pprint("FAILED TO CONNECT TO NETLINK\n");
        goto die_and_free_socket;
    }

    struct nl_cache *cache;
    if (rtnl_link_alloc_cache(sk, AF_INET, &cache)) {
        pprint("FAILED TO ALLOC LINK CACHE\n");
        goto die_and_close_socket;
    }

    int ifindex = rtnl_link_name2i(cache, wireless_interface);
    if (!ifindex) {
        pprint("FAILED TO RESOLVE IFINDEX FROM NAME\n");
        goto die_and_free_cache;
    }

    struct nl_addr *nla;
    if (nl_addr_parse(addr_to_set, AF_INET, &nla)) {
        pprint("FAILED TO PARSE ADDRESS\n");
        goto die_and_free_cache;
    }

    struct rtnl_addr *addr = rtnl_addr_alloc();

    rtnl_addr_set_ifindex(addr, ifindex);
    rtnl_addr_set_local(addr, nla);
    
    if (rtnl_addr_add(sk, addr, 0)) {
        pprint("FAILED TO ADD IP TO INTERFACE\n");
    } else {
        ret = VANILLA_SUCCESS;
    }

die_and_free_nl_addr:
    nl_addr_put(nla);

die_and_free_rtnl_addr:
    rtnl_addr_put(addr);

die_and_free_cache:
    nl_cache_free(cache);

die_and_close_socket:
    nl_close(sk);

die_and_free_socket:
    nl_socket_free(sk);

    return ret;
}

int do_dhcp(const char *wireless_interface)
{
    int old_stdout = dup(fileno(stdout));

    int dhcp_pipes[2];
    if (pipe2(dhcp_pipes, 0)) {
        print_info("FAILED TO CREATE DHCP PIPE: %i", errno);
        return VANILLA_ERROR;
    }

    if (dup2(dhcp_pipes[1], STDOUT_FILENO) == -1) {
        print_info("FAILED TO DUP2 STDOUT: %i", errno);
        return VANILLA_ERROR;
    }
    const char *udhcpc_argv[] = {
        "-i", wireless_interface,
        "-v",
        "-q",
        "-f",
        "-s", "/dev/null",
        NULL};
    
    int r = udhcpc_main(udhcpc_argv);

    // Restore old stdout fileno
    dup2(old_stdout, fileno(stdout));

    if (r) {
        print_info("FAILED TO RUN DHCP ON %s", wireless_interface);
        return VANILLA_ERROR;
    }

    print_info("DHCP ESTABLISHED");

    fflush(stdout);

    char buf[1024];
    read(dhcp_pipes[0], buf, sizeof(buf));
    print_info("DHCP OUTPUT: %s", buf);

    if (apply_addr_on_intf(wireless_interface, "192.168.1.11") != VANILLA_SUCCESS) {
        return VANILLA_ERROR;
    }

    return VANILLA_SUCCESS;
}

void *do_connect(void *data)
{
    struct sync_args *args = (struct sync_args *) data;

    while (1) {
        while (!wpa_ctrl_pending(args->ctrl)) {
            sleep(2);
            print_info("WAITING FOR CONNECTION");

            if (is_interrupted()) return THREADRESULT(VANILLA_ERROR);
        }

        char buf[1024];
        size_t actual_buf_len = sizeof(buf);
        wpa_ctrl_recv(args->ctrl, buf, &actual_buf_len);
        if (!strstr(buf, "CTRL-EVENT-BSS-ADDED")
            && !strstr(buf, "CTRL-EVENT-BSS-REMOVED")) {
            print_info("CONN RECV: %.*s", actual_buf_len, buf);
        }

        if (memcmp(buf, "<3>CTRL-EVENT-CONNECTED", 23) == 0) {
            break;
        }

        if (is_interrupted()) return THREADRESULT(VANILLA_ERROR);
    }

    print_info("CONNECTED TO CONSOLE");

    // Use DHCP on interface
    if (do_dhcp(args->wireless_interface) != VANILLA_SUCCESS) {
        return THREADRESULT(VANILLA_ERROR);
    }

    create_all_relays(args->wireless_interface);

    return THREADRESULT(VANILLA_SUCCESS);
}

void *vanilla_sync_with_console(void *data)
{
    struct sync_args *args = (struct sync_args *) data;

    const char *wireless_conf_file;

    FILE *config;
    wireless_conf_file = get_wireless_authenticate_config_filename();
    config = fopen(wireless_conf_file, "w");
    if (!config) {
        print_info("FAILED TO WRITE TEMP CONFIG: %s", wireless_conf_file);
        return THREADRESULT(VANILLA_ERROR);
    }

    fprintf(config, "ctrl_interface=%s\nupdate_config=1\n", wpa_ctrl_interface);
    fclose(config);

    args->start_routine = sync_with_console_internal;
    args->wireless_config = wireless_conf_file;

    void *ret = wpa_setup_environment(args);

    pthread_mutex_lock(&sync_mutex);
    sync_result_ready = 1;
    sync_result = (uint8_t) (uintptr_t) ret;
    pthread_mutex_unlock(&sync_mutex);

    return ret;
}

void *vanilla_connect_to_console(void *data)
{
    struct sync_args *args = (struct sync_args *) data;
    args->start_routine = do_connect;
    args->wireless_config = get_wireless_connect_config_filename();
    return wpa_setup_environment(args);
}

void vanilla_listen(const char *wireless_interface)
{
    int skt = open_socket(VANILLA_PIPE_CMD_SERVER_PORT);
    if (skt == -1) {
        pprint("Failed to open server socket\n");
        return;
    }
    uint32_t control_code;

    struct sockaddr_in addr = {0};
    socklen_t addr_size = sizeof(addr);

    pthread_mutex_init(&running_mutex, NULL);
    pthread_mutex_init(&action_mutex, NULL);
    pthread_mutex_init(&main_loop_mutex, NULL);
    pthread_mutex_init(&sync_mutex, NULL);

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    pthread_t stdin_thread;
    pthread_create(&stdin_thread, NULL, read_stdin, NULL);

    main_loop = 1;

    pthread_mutex_lock(&main_loop_mutex);

    pprint("READY\n");

    while (main_loop) {
        pthread_mutex_unlock(&main_loop_mutex);

        // If we got a sync result
        pthread_mutex_lock(&sync_mutex);
        if (sync_result_ready) {
            control_code = htonl(VANILLA_PIPE_CC_SYNC_STATUS | (sync_result & 0xFF));
            sendto(skt, &control_code, sizeof(control_code), 0, (struct sockaddr *) &addr, sizeof(addr));
            sync_result_ready = 0;
        }
        pthread_mutex_unlock(&sync_mutex);

        ssize_t r = recvfrom(skt, &control_code, sizeof(control_code), 0, (struct sockaddr *) &addr, &addr_size);
        if (r <= 0) {
            goto repeat_loop;
        }

        control_code = ntohl(control_code);

        if ((control_code >> 16) == (VANILLA_PIPE_CC_SYNC >> 16) || control_code == VANILLA_PIPE_CC_CONNECT) {
            print_info("RECEIVED SYNC/CONNECT SIGNAL");
            
            if (pthread_mutex_trylock(&action_mutex) == 0) {
                struct sync_args *args = malloc(sizeof(struct sync_args));
                args->wireless_interface = wireless_interface;
                args->code = control_code & 0xFFFF;
                args->start_routine = (control_code == VANILLA_PIPE_CC_CONNECT) ? vanilla_connect_to_console : vanilla_sync_with_console;
            
                client_address = addr.sin_addr;

                // Acknowledge
                control_code = htonl(VANILLA_PIPE_CC_BIND_ACK);
                sendto(skt, &control_code, sizeof(control_code), 0, (struct sockaddr *) &addr, sizeof(addr));

                pthread_t thread;
                int thread_ret;
                if (pthread_create(&thread, NULL, thread_handler, args) != 0) {
                    print_info("FAILED TO CREATE THREAD");
                    free(args);
                }
            } else {
                // Busy
                control_code = htonl(VANILLA_PIPE_CC_BUSY);
                sendto(skt, &control_code, sizeof(control_code), 0, (struct sockaddr *) &addr, sizeof(addr));
            }
        } else if (control_code == VANILLA_PIPE_CC_UNBIND) {
            print_info("RECEIVED UNBIND SIGNAL");
            pthread_mutex_lock(&running_mutex);
            running = 0;
            pthread_mutex_unlock(&running_mutex);
        }

repeat_loop:
        pthread_mutex_lock(&main_loop_mutex);
    }

    pthread_mutex_unlock(&main_loop_mutex);

    // Interrupt our stdin thread
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    pthread_kill(stdin_thread, SIGINT);

    pthread_mutex_destroy(&sync_mutex);
    pthread_mutex_destroy(&main_loop_mutex);
    pthread_mutex_destroy(&action_mutex);
    pthread_mutex_destroy(&running_mutex);
}

int vanilla_has_config()
{
    return (access(get_wireless_connect_config_filename(), F_OK) == 0);
}