#include "wpa.h"

#ifdef ANDROID
#include "android.h"
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <linux/version.h>
#include <net/if.h>
#include <linux/nl80211.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>
#include <netlink/route/addr.h>
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
#include "../ports.h"
#include "dhcp/dhcp.h"
#include "util.h"
#include "vanilla.h"
#include "wpa.h"

#ifdef USE_LIBNM
// NOTE: This header must be below others because it defines things that break other headers
#include <libnm/NetworkManager.h>
#endif

#include <stdbool.h> // Required by wpa_supplicant_i.h and utils/common.h
#include <utils/common.h>
#include <wpa_supplicant_i.h>

static pthread_mutex_t running_mutex;
static pthread_mutex_t main_loop_mutex;
static pthread_mutex_t action_mutex;
static pthread_mutex_t relay_mutex;
static int running = 0;
static int main_loop = 0;
static int relay_running = 0;

typedef struct {
    int from_socket;
    int to_socket;
    struct sockaddr_in to_address;
} relay_ports;

struct sync_args {
    const char *wireless_interface;
    const char *wireless_config;
    uint16_t code;
    unsigned char bssid[6];
    unsigned char psk[32];
    void *(*start_routine)(void *);
    struct wpa_ctrl *ctrl;
    struct wpa_ctrl *monitor;
    int local;
    int skt;
    struct sockaddr_in client;
};

struct relay_info {
    const char *wireless_interface;
    struct sockaddr_in client;
    in_port_t port;
};

#define THREADRESULT(x) ((void *) (uintptr_t) (x))

static void cleanup_runtime_directory();

static const char *get_wpa_runtime_directory()
{
#ifdef ANDROID
    static char runtime_directory[PATH_MAX];
    static int runtime_directory_init = 0;

    if (!runtime_directory_init) {
        umask(0077);

        if (snprintf(runtime_directory, sizeof(runtime_directory), "/data/local/tmp/vanilla-pipe-XXXXXX") >= sizeof(runtime_directory)
            || !mkdtemp(runtime_directory)) {
            nlprint("Failed to create Android runtime directory: %i", errno);
            return NULL;
        }

        runtime_directory_init = 1;
    }

    return runtime_directory;
#else
    return "/tmp";
#endif
}

static const char *get_wpa_ctrl_interface()
{
#ifdef ANDROID
    static char wpa_ctrl_interface[PATH_MAX];
    static int wpa_ctrl_interface_init = 0;

    if (!wpa_ctrl_interface_init) {
        umask(0077);

        if (snprintf(wpa_ctrl_interface, sizeof(wpa_ctrl_interface), "%s/wpa_supplicant_drc", get_wpa_runtime_directory()) >= sizeof(wpa_ctrl_interface)) {
            nlprint("Android runtime path is too long");
            return NULL;
        }

        if (mkdir(wpa_ctrl_interface, 0700) < 0) {
            nlprint("Failed to create %s: %i", wpa_ctrl_interface, errno);
            return NULL;
        }

        if (atexit(cleanup_runtime_directory) != 0) {
            nlprint("Failed to register Android runtime cleanup");
            return NULL;
        }

        wpa_ctrl_interface_init = 1;
    }

    return wpa_ctrl_interface;
#else
    return "/var/run/wpa_supplicant_drc";
#endif
}

static void cleanup_runtime_directory()
{
    char path[PATH_MAX];
    int length;

    length = snprintf(path, sizeof(path), "%s/vanilla_wpa_key.conf",
                      get_wpa_runtime_directory());
    if (length >= 0 && length < (int) sizeof(path))
        unlink(path);
    length = snprintf(path, sizeof(path), "%s/vanilla_wpa_connect.conf",
                      get_wpa_runtime_directory());
    if (length >= 0 && length < (int) sizeof(path))
        unlink(path);

    rmdir(get_wpa_ctrl_interface());
    rmdir(get_wpa_runtime_directory());
}

static const char *ext_logfile = 0;
void nlprint(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    if (ext_logfile) {
		va_list a2;
		va_copy(a2, args);

        FILE *f = fopen(ext_logfile, "a");
        if (f) {
            vfprintf(f, fmt, a2);
            fprintf(f, "\n");
            fclose(f);
        }

		va_end(a2);
    }

    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
}

int is_interrupted()
{
    pthread_mutex_lock(&running_mutex);
    int r = !running;
    pthread_mutex_unlock(&running_mutex);
    return r;
}

int are_relays_running()
{
    pthread_mutex_lock(&relay_mutex);
    int r = relay_running;
    pthread_mutex_unlock(&relay_mutex);
    return r;
}

void vanilla_pipe_wpa_msg(char *msg, size_t len)
{
    nlprint("%.*s", len, msg);
}

int wpa_ctrl_command(struct wpa_ctrl *ctrl, const char *cmd, char *buf,
                     size_t *buf_len)
{
    return wpa_ctrl_request(ctrl, cmd, strlen(cmd), buf, buf_len, NULL /* vanilla_pipe_wpa_msg */);
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

void interrupt()
{
    pthread_mutex_lock(&running_mutex);
    running = 0;
    pthread_mutex_unlock(&running_mutex);
}

void interrupt_relays()
{
    pthread_mutex_lock(&relay_mutex);
    relay_running = 0;
    pthread_mutex_unlock(&relay_mutex);
}

void sigint_handler(int signum)
{
    if (signum == SIGINT) {
        nlprint("RECEIVED INTERRUPT SIGNAL");
    } else if (signum == SIGTERM) {
        nlprint("RECEIVED TERMINATE SIGNAL");
    }
    quit_loop();
}

void *read_stdin(void *arg)
{
    char line[256];
    ssize_t read_size = 0;
	fd_set fds;
	struct timeval tv = {0, 10000}; // 10ms

    pthread_mutex_lock(&main_loop_mutex);
	while (main_loop) {
		pthread_mutex_unlock(&main_loop_mutex);

		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);

		int sel = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
		if (sel > 0) {
			char c;
			ssize_t s = read(STDIN_FILENO, &c, 1);
			if (s > 0) {
				if (c == '\n') {
					// Add null terminator
					line[read_size] = 0;

					// Parse line
					if (!strcasecmp(line, "quit") || !strcasecmp(line, "exit") || !strcasecmp(line, "bye")) {
						quit_loop();
					}

					read_size = 0;
				} else {
					line[read_size] = c;
					read_size = (read_size + 1) % sizeof(line);
				}
			} else if (s == 0) {
                // Frontend closed its end of the pipe, shut down
				quit_loop();
				pthread_mutex_lock(&main_loop_mutex);
				break;
			} else if (s < 0) {
				perror("read()");
			}
		} else if (sel < 0) {
			perror("select()");
		} else if (sel == 0) {
            // Don't thrash while waiting for input
            sleep(1);
        }
		pthread_mutex_lock(&main_loop_mutex);
	}
	pthread_mutex_unlock(&main_loop_mutex);

    return NULL;
}

void *start_wpa(void *arg)
{
    return THREADRESULT(wpa_supplicant_run((struct wpa_global *) arg));
}

ssize_t run_process_and_read_stdout(const char **args, char *read_buffer, size_t read_buffer_len)
{
	// Create pipe so we can read from the forked child
	int pipefd[2];
	if (pipe(pipefd) < 0) {
		nlprint("Failed to create subprocess pipe: %i", errno);
		return -1;
	}

	// Perform fork
	pid_t pid = fork();

	// Handle fork
	switch (pid) {
	case -1:
		// Fork failed for some reason, report the errno
		nlprint("Failed to fork to run process: %i", errno);
		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	case 0:
		// We are the child process, go ahead and run exec
		close(pipefd[0]); // Close reading pipe

		dup2(pipefd[1], STDOUT_FILENO); // Set stdout to write
		// dup2(pipefd[1], STDERR_FILENO); // Set stderr to write

		close(pipefd[1]); // Done with this for now

		execvp(args[0], (char * const *) args);

		// Exit immediately if for some reason execvp failed
		_exit(127);
	default:
		// We are the parent process. Read from stdout until EOF.

		close(pipefd[1]); // Close write, don't need it

		size_t stored_len = 0;
		while (1) {
			char discard_buffer[512];
			char *destination = discard_buffer;
			size_t destination_len = sizeof(discard_buffer);

			if (read_buffer && stored_len < read_buffer_len) {
				destination = read_buffer + stored_len;
				destination_len = read_buffer_len - stored_len;
			}

			ssize_t read_len = read(pipefd[0], destination, destination_len);
			if (read_len < 0 && errno == EINTR) {
				continue;
            }
			if (read_len <= 0) {
				break;
            }
			if (destination != discard_buffer) {
				stored_len += read_len;
            }
		}

		int status;
		if (waitpid(pid, &status, 0) == -1) {
			nlprint("Failed to waitpid: %i", errno);
			return -1;
		}

		close(pipefd[0]); // Close read, we're done reading

		if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
			return stored_len;
		} else {
			nlprint("Subprocess failed with status: 0x%x", status);
			return -1;
		}
	}
}

void set_signals()
{
	struct sigaction sa;
	sa.sa_handler = sigint_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
}

void *wpa_setup_environment(void *data)
{
    void *ret = THREADRESULT(VANILLA_ERR_GENERIC);

    struct sync_args *args = (struct sync_args *) data;

    // Start modified WPA supplicant
    struct wpa_params params = {0};
    params.wpa_debug_level = MSG_INFO;

    struct wpa_interface interface = {0};
    interface.driver = "nl80211";
    interface.ifname = args->wireless_interface;
    interface.confname = args->wireless_config;

    struct wpa_global *wpa = wpa_supplicant_init(&params);
    if (!wpa) {
        nlprint("FAILED TO INIT WPA SUPPLICANT");
        goto die;
    }

    pthread_t wpa_thread;
    int wpa_thread_started = 0;

    struct wpa_supplicant *wpa_s = wpa_supplicant_add_iface(wpa, &interface, NULL);
    if (!wpa_s) {
        nlprint("FAILED TO ADD WPA IFACE");
        goto die_and_kill;
    }

    if (pthread_create(&wpa_thread, NULL, start_wpa, wpa) != 0) {
        nlprint("FAILED TO START WPA SUPPLICANT THREAD");
        goto die_and_kill;
    }
    wpa_thread_started = 1;

    // Get control interface
    char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s/%s", get_wpa_ctrl_interface(),
             args->wireless_interface);
    struct wpa_ctrl *ctrl;
    while (1) {
#ifdef ANDROID
        // wpa_ctrl_open creates a client socket in /tmp by default which doesn't
        // exist on Android, so ensure it uses our runtime dir
        ctrl = wpa_ctrl_open2(buf, get_wpa_runtime_directory());
#else
        ctrl = wpa_ctrl_open(buf);
#endif
        if (ctrl)
            break;

        if (is_interrupted()) goto die_and_kill;
        nlprint("WAITING FOR CTRL INTERFACE: %i", errno);
        sleep(1);
    }

    struct wpa_ctrl *monitor;
    while (1) {
#ifdef ANDROID
        monitor = wpa_ctrl_open2(buf, get_wpa_runtime_directory());
#else
        monitor = wpa_ctrl_open(buf);
#endif
        if (monitor)
            break;

        if (is_interrupted())
            goto die_and_close_ctrl;
        nlprint("WAITING FOR WPA MONITOR INTERFACE: %i", errno);
        sleep(1);
    }

    if (is_interrupted() || wpa_ctrl_attach(monitor) < 0) {
        nlprint("FAILED TO ATTACH TO WPA");
        goto die_and_close_monitor;
    }

	// wpa_supplicant_run may have replaced our signals, so lets re-set them
    // Wait until after we get ctrl interface because wpa_supplicant_run
    // runs in a separate thread
	set_signals();

    args->ctrl = ctrl;
    args->monitor = monitor;
    ret = args->start_routine(args);

die_and_detach:
    wpa_ctrl_detach(monitor);

die_and_close_monitor:
    wpa_ctrl_close(monitor);

die_and_close_ctrl:
    wpa_ctrl_close(ctrl);

die_and_kill:
    if (wpa_thread_started) {
        wpa_supplicant_terminate_proc(wpa);
        pthread_join(wpa_thread, NULL);
    }
    wpa_supplicant_deinit(wpa);

die:
    return ret;
}

static int netmask_to_prefixlen(struct in_addr netmask)
{
    uint32_t mask = ntohl(netmask.s_addr);
    int prefixlen = 0;
    int found_zero = 0;

    for (int bit = 31; bit >= 0; bit--) {
        if (mask & (1u << bit)) {
            if (found_zero) {
                return -1;
            }
            prefixlen++;
        } else {
            found_zero = 1;
        }
    }
    return prefixlen;
}

static int call_dhcp(const char *network_interface, struct nl_sock *nl)
{
    int ret = VANILLA_ERR_GENERIC;

    if (!nl)
        return ret;

    sdhcp_lease_t lease;
    if (sdhcp_acquire(network_interface, 5000, 3, &lease) < 0) {
        nlprint("DHCP ACQUISITION FAILED ON %s: %i", network_interface, errno);
        return ret;
    }

    int ifindex = if_nametoindex(network_interface);
    if (ifindex == 0)
        return ret;

    int prefixlen = netmask_to_prefixlen(lease.subnet_mask);
    if (prefixlen < 0)
        return ret;

    struct nl_addr *local =
        nl_addr_build(AF_INET, &lease.yiaddr, sizeof(lease.yiaddr));
    if (!local)
        return ret;
    nl_addr_set_prefixlen(local, prefixlen);

    struct rtnl_addr *addr = rtnl_addr_alloc();
    if (!addr) {
        nl_addr_put(local);
        return ret;
    }

    rtnl_addr_set_ifindex(addr, ifindex);
    rtnl_addr_set_local(addr, local);

    struct nl_msg *msg = NULL;
    if (rtnl_addr_build_add_request(
            addr, NLM_F_CREATE | NLM_F_REPLACE, &msg) < 0) {
        rtnl_addr_put(addr);
        nl_addr_put(local);
        return ret;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
    // Make this route the lowest possible priority so the system doesn't favor it over other connections
    nla_put_u32(msg, IFA_RT_PRIORITY, UINT32_MAX);
#endif

    int address_result = nl_send_auto_complete(nl, msg);
    if (address_result >= 0)
        address_result = nl_wait_for_ack(nl);

    nlmsg_free(msg);
    rtnl_addr_put(addr);
    if (address_result < 0) {
        nlprint("FAILED TO CONFIGURE DHCP ADDRESS: %s",
                nl_geterror(address_result));
        nl_addr_put(local);
        return ret;
    }

#ifdef ANDROID
    // Android prevents user apps from calling SO_BINDTODEVICE, so while we're
    // root, we'll create a route that the user-side can use to effectively
    // reach the Wii U
    if (vanilla_android_install_console_routing(nl, ifindex, local) < 0) {
        nl_addr_put(local);
        return ret;
    }
#endif

    nl_addr_put(local);
    return VANILLA_SUCCESS;
}

int nl80211_set_power_save(struct nl_sock *nl, int nl80211_id, int ifindex, int ps_state)
{
    struct nl_msg *msg = nlmsg_alloc();
    if (!msg) {
        return -1;
    }

    genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, nl80211_id, 0, 0, NL80211_CMD_SET_POWER_SAVE, 0);
    nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifindex);
    nla_put_u32(msg, NL80211_ATTR_PS_STATE, ps_state);

    int ret = nl_send_auto_complete(nl, msg);
    nlmsg_free(msg);
    if (ret < 0) {
        return ret;
    }

    return nl_wait_for_ack(nl);
}

int nl80211_get_power_save_cb(struct nl_msg *msg, void *arg)
{
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *attrs[NL80211_ATTR_MAX + 1];

    if (nla_parse(attrs, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL) == 0) {
        if (attrs[NL80211_ATTR_PS_STATE]) {
            *((int *) arg) = nla_get_u32(attrs[NL80211_ATTR_PS_STATE]);
        }
    }

    return NL_SKIP;
}

int nl80211_get_power_save(struct nl_sock *nl, int nl80211_id, int ifindex, int *ps_state)
{
    struct nl_msg *msg = nlmsg_alloc();
    if (!msg) {
        return -1;
    }

    genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, nl80211_id, 0, 0, NL80211_CMD_GET_POWER_SAVE, 0);
    nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifindex);

    *ps_state = -1;
    nl_socket_modify_cb(nl, NL_CB_VALID, NL_CB_CUSTOM, nl80211_get_power_save_cb, ps_state);

    int ret = nl_send_auto_complete(nl, msg);
    nlmsg_free(msg);
    if (ret < 0) {
        return ret;
    }

    ret = nl_recvmsgs_default(nl);
    if (ret < 0) {
        return ret;
    }

    return (*ps_state == -1) ? -1 : 0;
}

void *do_relay(void *data)
{
    relay_ports *ports = (relay_ports *) data;
    char buf[4096];
    ssize_t read_size;
    while (are_relays_running()) {
        read_size = recv(ports->from_socket, buf, sizeof(buf), 0);
        if (read_size <= 0) {
            continue;
        }

        if (sendto(ports->to_socket, buf, read_size, 0, (const struct sockaddr *) &ports->to_address,  sizeof(ports->to_address)) == -1) {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &ports->to_address.sin_addr, ip, sizeof(ip));
            nlprint("FAILED TO SENDTO %s:%u (%i)", ip, ntohs(ports->to_address.sin_port), errno);
        }
    }
    return NULL;
}

relay_ports create_ports(int from_socket, int to_socket,
                         const struct sockaddr_in *to_addr)
{
    relay_ports ports;
    ports.from_socket = from_socket;
    ports.to_socket = to_socket;
    ports.to_address = *to_addr;
    return ports;
}

int open_socket(int local, in_port_t port)
{
    int skt = socket(AF_INET, SOCK_DGRAM, 0);
    if (skt == -1) {
        return -1;
    }

    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = local ? htonl(INADDR_LOOPBACK) : INADDR_ANY;
    sa.sin_port = htons(port);
    struct timeval tv = {0};
    tv.tv_usec = 250000;
    setsockopt(skt, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (bind(skt, (const struct sockaddr *) &sa, sizeof(sa)) == -1) {
        nlprint("FAILED TO BIND PORT %u: %i", port, errno);
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
    int from_console = open_socket(0, port);
    if (from_console == -1) {
        goto close;
    }

    setsockopt(from_console, SOL_SOCKET, SO_BINDTODEVICE, info->wireless_interface, strlen(info->wireless_interface));

    // Open an incoming port from the frontend
    int from_frontend = open_socket(0, port - 100);
    if (from_frontend == -1) {
        goto close_console_connection;
    }

    struct sockaddr_in console_addr = {0};
    console_addr.sin_family = AF_INET;
    console_addr.sin_addr.s_addr = inet_addr("192.168.1.10");
    console_addr.sin_port = htons(port - 100);

    struct sockaddr_in frontend_addr = info->client;
    frontend_addr.sin_port = htons(port);

    // nlprint("ENTERING MAIN LOOP");
    while (are_relays_running()) {
        nlprint("STARTED RELAYS");
        relay_ports console_to_frontend = create_ports(
            from_console, from_frontend, &frontend_addr);
        relay_ports frontend_to_console = create_ports(
            from_frontend, from_console, &console_addr);

        pthread_t a_thread, b_thread;
        pthread_create(&a_thread, NULL, do_relay, &console_to_frontend);
        pthread_create(&b_thread, NULL, do_relay, &frontend_to_console);

        pthread_join(a_thread, NULL);
        pthread_join(b_thread, NULL);

        nlprint("STOPPED RELAYS");

        // nlprint("RELAY EXITED");
    }

    ret = 0;

close_frontend_connection:
    close(from_frontend);

close_console_connection:
    close(from_console);

close:
    return THREADRESULT(ret);
}

int check_for_disconnection(struct sync_args *args)
{
    vanilla_pipe_command_t cmd;
    char buf[1024];
    size_t buf_len = sizeof(buf);
    if (wpa_ctrl_recv(args->monitor, buf, &buf_len) == 0) {
        if (!memcmp(buf, "<3>CTRL-EVENT-DISCONNECTED", 26)) {
            nlprint("Wii U disconnected, attempting to re-connect...");

            // Let client know we lost connection
            cmd.control_code = VANILLA_PIPE_CC_DISCONNECTED;
            sendto(args->skt, &cmd, sizeof(cmd.control_code), 0,
                   (const struct sockaddr *) &args->client,
                   sizeof(args->client));

            return 1;
        }
    }

    return 0;
}

void create_all_relays(struct sync_args *args)
{
    pthread_t vid_thread, aud_thread, msg_thread, cmd_thread, hid_thread;
    struct relay_info vid_info, aud_info, msg_info, cmd_info, hid_info;

    if (!args->local) {
        // Set common info for all
        vid_info.wireless_interface = aud_info.wireless_interface = msg_info.wireless_interface = cmd_info.wireless_interface = hid_info.wireless_interface = args->wireless_interface;
        vid_info.client = aud_info.client = msg_info.client = cmd_info.client = hid_info.client = args->client;

        vid_info.port = PORT_VID;
        aud_info.port = PORT_AUD;
        msg_info.port = PORT_MSG;
        cmd_info.port = PORT_CMD;
        hid_info.port = PORT_HID;

        // Enable relays
        relay_running = 1;

        pthread_create(&vid_thread, NULL, open_relay, &vid_info);
        pthread_create(&aud_thread, NULL, open_relay, &aud_info);
        pthread_create(&msg_thread, NULL, open_relay, &msg_info);
        pthread_create(&cmd_thread, NULL, open_relay, &cmd_info);
        pthread_create(&hid_thread, NULL, open_relay, &hid_info);
    }

    // Notify client that we are connected
    vanilla_pipe_command_t cmd;
    cmd.control_code = VANILLA_PIPE_CC_CONNECTED;
    sendto(args->skt, &cmd, sizeof(cmd.control_code), 0, (const struct sockaddr *) &args->client, sizeof(args->client));

    while (!is_interrupted()) {
        if (check_for_disconnection(args)) {
            break;
        }

        // wpa_ctrl_recv is non-blocking, so reduce thrashing by sleeping here
        sleep(1);
    }

    if (!args->local) {
        // Stop all relays
        interrupt_relays();

        pthread_join(vid_thread, NULL);
        pthread_join(aud_thread, NULL);
        pthread_join(msg_thread, NULL);
        pthread_join(cmd_thread, NULL);
        pthread_join(hid_thread, NULL);
    }
}

void *thread_handler(void *data)
{
    struct sync_args *args = (struct sync_args *) data;

    pthread_mutex_lock(&running_mutex);
    running = 1;
    pthread_mutex_unlock(&running_mutex);

    void *ret = args->start_routine(data);
    int status = (int) (intptr_t) ret;

    if (status != VANILLA_SUCCESS && !is_interrupted()) {
        vanilla_pipe_command_t cmd;
        cmd.control_code = VANILLA_PIPE_CC_STATUS;
        cmd.status.status = htonl(status);
        if (sendto(args->skt, &cmd, sizeof(cmd.control_code) + sizeof(cmd.status), 0, (const struct sockaddr *) &args->client, sizeof(args->client)) < 0) {
            nlprint("FAILED TO SEND ACTION ERROR %i: %i", status, errno);
        }
    }

    free(args);

    interrupt();

    // Locked by calling thread
    pthread_mutex_unlock(&action_mutex);

    return ret;
}

char wireless_authenticate_config_filename[1024] = {0};
char wireless_connect_config_filename[1024] = {0};

size_t get_home_directory_file(const char *filename, char *buf, size_t buf_size)
{
    return snprintf(buf, buf_size, "%s/%s", get_wpa_runtime_directory(), filename);
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

void str_to_bytes(const char *str, int advance, unsigned char *output, size_t output_size)
{
    char c[3];
    c[2] = 0;
    for (size_t i = 0; i < output_size; i++) {
        memcpy(c, str + i * (2 + advance), 2);
        output[i] = strtoul(c, 0, 16);
    }
}

void bytes_to_str(unsigned char *data, size_t data_size, const char *separator, char *output)
{
    size_t output_advance = 0;
    size_t separator_size = separator ? strlen(separator) : 0;
    for (size_t i = 0; i < data_size; i++) {
        if (separator_size > 0 && i > 0) {
            strncpy(output + output_advance, separator, separator_size);
            output_advance += separator_size;
        }
        unsigned char byte = data[i];
        sprintf(output + output_advance, "%02x", byte & 0xFF);
        output_advance += 2;
    }
}

int create_connect_config(const char *filename, unsigned char *bssid, unsigned char *psk)
{
    FILE *out_file = fopen(filename, "w");
    if (!out_file) {
        nlprint("FAILED TO OPEN OUTPUT CONFIG FILE");
        return VANILLA_ERR_GENERIC;
    }

    static const char *template =
        "ctrl_interface=%s\n"
        "ap_scan=1\n"
        "scan_cur_freq=1\n"
        "\n"
        "network={\n"
        "	scan_ssid=1\n"
        "	bssid=%s\n"
        "	ssid=\"%s\"\n"
        "	psk=%s\n"
        "	proto=RSN\n"
        "	key_mgmt=WPA-PSK\n"
        "	pairwise=CCMP GCMP\n"
        "	group=CCMP GCMP TKIP\n"
        "	auth_alg=OPEN\n"
        "	pbss=2\n"
        "}\n"
        "\n";

    char bssid_str[18];
    char ssid_str[17];
    char psk_str[65];

    memcpy(ssid_str, "WiiU", 4);

    bytes_to_str(bssid, sizeof(vanilla_bssid_t), 0, ssid_str + 4);
    bytes_to_str(bssid, sizeof(vanilla_bssid_t), ":", bssid_str);
    bytes_to_str(psk, sizeof(vanilla_psk_t), 0, psk_str);

    fprintf(out_file, template, get_wpa_ctrl_interface(), bssid_str, ssid_str, psk_str);
    fclose(out_file);

    return VANILLA_SUCCESS;
}

ssize_t send_ping_to_client(struct sync_args *args)
{
    vanilla_pipe_command_t cmd;
    cmd.control_code = VANILLA_PIPE_CC_PING;
    return sendto(args->skt, &cmd, sizeof(cmd.control_code), 0, (const struct sockaddr *) &args->client, sizeof(args->client));
}

static int wait_for_scan_results_event(struct sync_args *args)
{
    static const int poll_interval_us = 100000;
    static const int timeout_seconds = 15;
    char event[1024];

    for (int elapsed = 0; elapsed < timeout_seconds * 1000000; elapsed += poll_interval_us) {
        if (is_interrupted()) {
            return -1;
        }

        int pending = wpa_ctrl_pending(args->monitor);
        if (pending < 0) {
            return -1;
        }
        if (pending > 0) {
            size_t event_len = sizeof(event);
            if (wpa_ctrl_recv(args->monitor, event, &event_len) < 0) {
                return -1;
            }
            if (event_len >= strlen("<3>CTRL-EVENT-SCAN-RESULTS")
                && !memcmp(event, "<3>CTRL-EVENT-SCAN-RESULTS", strlen("<3>CTRL-EVENT-SCAN-RESULTS"))) {
                return 0;
            }
            continue;
        }

        usleep(poll_interval_us);
    }

    return -1;
}

void *sync_with_console_internal(void *data)
{
    struct sync_args *args = (struct sync_args *) data;

    char buf[16384];
    const size_t buf_len = sizeof(buf);

    int ret = VANILLA_ERR_GENERIC;
    char bssid[18];
    while (1) {
        size_t actual_buf_len;

        if (is_interrupted()) goto exit_loop;

        // Request scan from hardware
        while (1) {
            if (is_interrupted()) goto exit_loop;

            if (send_ping_to_client(args) == -1) {
                // Client has probably disconnected
                interrupt();
                goto exit_loop;
            }

            // nlprint("SCANNING");
            actual_buf_len = buf_len;
            int command_status =
                wpa_ctrl_command(args->ctrl, "SCAN", buf, &actual_buf_len);

            if (command_status < 0) {
                nlprint("SCAN COMMAND TIMED OUT (%i), RETRYING",
                        command_status);
                sleep(5);
                continue;
            }

            if (!memcmp(buf, "FAIL-BUSY", 9)) {
                nlprint("SCAN BUSY, WAITING FOR SUPPLICANT");
                sleep(5);
            } else if (!memcmp(buf, "OK", 2)) {
                break;
            } else {
                nlprint("UNKNOWN SCAN RESPONSE: %.*s (RETRYING)", actual_buf_len, buf);
                sleep(5);
            }
        }

        if (wait_for_scan_results_event(args) < 0) {
            nlprint("TIMED OUT WAITING FOR SCAN COMPLETION, RETRYING");
            continue;
        }

        actual_buf_len = buf_len;
        if (wpa_ctrl_command(args->ctrl, "SCAN_RESULTS", buf,
                             &actual_buf_len) < 0) {
            nlprint("FAILED TO READ SCAN RESULTS, RETRYING");
            sleep(2);
            continue;
        }
        nlprint("RECEIVED SCAN RESULTS");

        if (actual_buf_len < buf_len) {
            buf[actual_buf_len] = '\0';
        } else {
            buf[buf_len - 1] = '\0';
        }

        const char *line = strtok(buf, "\n");
        while (line) {
            if (is_interrupted()) goto exit_loop;

            if (strstr(line, "WiiU") && strstr(line, "_STA1")) {
                nlprint("FOUND WII U, STARTING CONSOLE PAIRING");

                // Make copy of bssid for later
                strncpy(bssid, line, sizeof(bssid));
                bssid[17] = '\0';

                char wps_buf[100];
                snprintf(wps_buf, sizeof(wps_buf), "WPS_PIN %.*s %04d5678", 17, bssid, args->code);

                size_t actual_buf_len = buf_len;
                if (wpa_ctrl_command(args->ctrl, wps_buf, buf, &actual_buf_len) < 0) {
                    nlprint("FAILED TO START CONSOLE PAIRING");
                    break;
                }

                static const int max_wait = 60;
                int wait_count = 0;
                int cred_received = 0;

                while (!is_interrupted()) {
                    // Send a ping to the client to say we're still here
                    if (send_ping_to_client(args) == -1) {
                        // Client has probably disconnected
                        interrupt();
                        goto exit_loop;
                    }

                    int pending = wpa_ctrl_pending(args->monitor);
                    if (pending <= 0) {
                        if (pending < 0) {
                            nlprint("FAILED TO POLL PAIRING EVENTS");
                            goto exit_loop;
                        }
                        // If we haven't gotten any information yet, wait one second and try again
                        wait_count++;
                        if (wait_count == max_wait) {
                            nlprint("PAIRING TIMED OUT, CANCELLING BEFORE RESCAN");

                            actual_buf_len = buf_len;
                            int cancel_status = wpa_ctrl_command(
                                args->ctrl, "WPS_CANCEL", buf,
                                &actual_buf_len);
                            if (cancel_status < 0) {
                                nlprint("SUPPLICANT CONTROL LOOP IS "
                                        "UNRESPONSIVE (%i)", cancel_status);
                                goto exit_loop;
                            }

                            nlprint("PAIRING CANCELLED, RETURNING TO SCANNING");
                            sleep(1);
                            break;
                        } else {
                            sleep(1);
                            continue;
                        }
                    } else {
                        wait_count = 0;
                    }

                    actual_buf_len = buf_len;
                    wpa_ctrl_recv(args->monitor, buf, &actual_buf_len);
                    if (!strstr(buf, "CTRL-EVENT-BSS-ADDED")
                        && !strstr(buf, "CTRL-EVENT-BSS-REMOVED")) {
                        nlprint("CRED RECV: %.*s", actual_buf_len, buf);
                    }

                    if (!memcmp("<3>WPS-CRED-RECEIVED", buf, 20)) {
                        nlprint("RECEIVED AUTHENTICATION FROM CONSOLE");
                        cred_received = 1;
                        break;
                    }
                }

                if (cred_received) {
                    vanilla_pipe_command_t cmd;
                    cmd.control_code = VANILLA_PIPE_CC_SYNC_SUCCESS;

                    // Tell wpa_supplicant to save config (this seems to be the only way to retrieve the PSK)
                    actual_buf_len = buf_len;
                    nlprint("SAVING CONFIG", actual_buf_len, buf);
                    if (wpa_ctrl_command(args->ctrl, "SAVE_CONFIG", buf,
                                         &actual_buf_len) < 0) {
                        nlprint("FAILED TO SAVE PAIRING CONFIGURATION");
                        goto exit_loop;
                    }

                    // Retrieve BSSID and PSK from saved config
                    FILE *in_file = fopen(get_wireless_authenticate_config_filename(), "r");
                    if (in_file) {
                        // Convert PSK from string to bytes
                        int len;
                        char buf[150];
                        while (len = read_line_from_file(in_file, buf, sizeof(buf))) {
                            if (memcmp("\tpsk=", buf, 5) == 0) {
                                str_to_bytes(buf + 5, 0, cmd.connection.psk.psk, sizeof(cmd.connection.psk.psk));
                                break;
                            }
                        }

                        fclose(in_file);

                        // Convert BSSID from string to bytes
                        str_to_bytes(bssid, 1, cmd.connection.bssid.bssid, sizeof(cmd.connection.bssid.bssid));

                        sendto(args->skt, &cmd,
                               sizeof(cmd.control_code) +
                                   sizeof(cmd.connection),
                               0, (const struct sockaddr *) &args->client,
                               sizeof(args->client));

                        ret = VANILLA_SUCCESS;
                    } else {
                        // TODO: Return error to the frontend
                        nlprint("FAILED TO OPEN INPUT CONFIG FILE TO RETRIEVE PSK");
                    }

                    goto exit_loop;
                }

                break;
            }
            line = strtok(NULL, "\n");
        }
    }

exit_loop:
    return THREADRESULT(ret);
}

void *do_connect(void *data)
{
    struct sync_args *args = (struct sync_args *) data;

    while (!is_interrupted()) {
        while (1) {
            while (wpa_ctrl_pending(args->monitor) <= 0) {
                sleep(2);
                nlprint("WAITING FOR CONNECTION");

                if (is_interrupted()) return THREADRESULT(VANILLA_ERR_GENERIC);
            }

            char buf[1024];
            size_t actual_buf_len = sizeof(buf);
            wpa_ctrl_recv(args->monitor, buf, &actual_buf_len);
            if (!strstr(buf, "CTRL-EVENT-BSS-ADDED")
                && !strstr(buf, "CTRL-EVENT-BSS-REMOVED")) {
                nlprint("CONN RECV: %.*s", actual_buf_len, buf);
            }

            if (memcmp(buf, "<3>CTRL-EVENT-CONNECTED", 23) == 0) {
                break;
            }

            if (is_interrupted()) return THREADRESULT(VANILLA_ERR_GENERIC);
        }

        nlprint("CONNECTED TO CONSOLE");

        // Use DHCP on interface
        struct nl_sock *dhcp_socket = nl_socket_alloc();
        if (!dhcp_socket) {
            nlprint("FAILED TO ALLOC DHCP NL_SOCK");
            return THREADRESULT(VANILLA_ERR_GENERIC);
        }

        int nlr = nl_connect(dhcp_socket, NETLINK_ROUTE);
        if (nlr < 0) {
            nlprint("FAILED TO CONNECT DHCP NL_SOCK: %s", nl_geterror(nlr));
            nl_socket_free(dhcp_socket);
            return THREADRESULT(VANILLA_ERR_GENERIC);
        }

        int r = call_dhcp(args->wireless_interface, dhcp_socket);
        if (r != VANILLA_SUCCESS) {
            nl_close(dhcp_socket);
            nl_socket_free(dhcp_socket);

            // For some reason, DHCP did not succeed. Determine if it's because
            // the Wi-Fi disconnected mid-handshake.
            if (check_for_disconnection(args)) {
                // If so, start this loop over again
                continue;
            }

            // DHCP failed for some other reason. Since we don't know what it is,
            // treat it as a fatal error.
            nlprint("FAILED TO RUN DHCP ON %s", args->wireless_interface);
            return THREADRESULT(r);
        } else {
            nlprint("DHCP ESTABLISHED");
        }

        create_all_relays(args);

#ifdef ANDROID
        vanilla_android_remove_console_routing(dhcp_socket);
#endif
        nl_close(dhcp_socket);
        nl_socket_free(dhcp_socket);
    }

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
        nlprint("FAILED TO WRITE TEMP CONFIG: %s", wireless_conf_file);
        return THREADRESULT(VANILLA_ERR_GENERIC);
    }

    fprintf(config, "ctrl_interface=%s\nupdate_config=1\n",
            get_wpa_ctrl_interface());
    fclose(config);

    args->start_routine = sync_with_console_internal;
    args->wireless_config = wireless_conf_file;

    return wpa_setup_environment(args);
}

void *vanilla_connect_to_console(void *data)
{
    struct sync_args *args = (struct sync_args *) data;

    args->start_routine = do_connect;
    args->wireless_config = get_wireless_connect_config_filename();

    create_connect_config(args->wireless_config, args->bssid, args->psk);

    return wpa_setup_environment(args);
}

int uninstall_polkit()
{
	unlink(POLKIT_ACTION_DST);
	unlink(POLKIT_RULE_DST);
}

int install_polkit()
{
	static const char *ACTION_TEMPLATE =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<!DOCTYPE policyconfig PUBLIC \"-//freedesktop//DTD PolicyKit Policy Configuration 1.0//EN\" \"http://www.freedesktop.org/standards/PolicyKit/1/policyconfig.dtd\">\n"
		"<policyconfig>\n"
		"  <vendor>MattKC</vendor>\n"
		"  <vendor_url>https://mattkc.com</vendor_url>\n"
		"  <action id=\"com.mattkc.vanilla\">\n"
		"    <description>Run Vanilla Pipe as root</description>\n"
		"    <message>Authentication is required to run Vanilla Pipe as root</message>\n"
		"    <defaults>\n"
		"      <allow_any>auth_admin</allow_any>\n"
		"      <allow_inactive>auth_admin</allow_inactive>\n"
		"      <allow_active>auth_admin</allow_active>\n"
		"    </defaults>\n"
		"    <annotate key=\"org.freedesktop.policykit.exec.path\">%s</annotate>\n"
		"    <annotate key=\"org.freedesktop.policykit.exec.allow_gui\">true</annotate>\n"
		"  </action>\n"
		"</policyconfig>\n";

	static const char *RULE_TEMPLATE =
		"/**\n"
		" * Allow all users to run vanilla-pipe as root without having to enter a password\n"
		" *\n"
		" * This provides convenience, especially on platforms more suited for touch\n"
		" * controls, however it could be dangerous to allow a program unrestricted\n"
		" * administrator access, so it should be used with caution.\n"
		" */\n"
		"polkit.addRule(function(action, subject) {\n"
		"    if (action.id == \"com.mattkc.vanilla\") {\n"
		"        return polkit.Result.YES;\n"
		"    }\n"
		"});\n";

	// Get current filename
	char exe[4096];
	ssize_t link_len = readlink("/proc/self/exe", exe, sizeof(exe));
	exe[link_len] = 0;

	// If on the Steam Deck, ensure the system partition is writeable
	run_process_and_read_stdout((const char *[]) {"steamos-readonly", "disable", NULL}, 0, 0);

	int ret = VANILLA_ERR_GENERIC;

	FILE *action = fopen(POLKIT_ACTION_DST, "w");
	FILE *rule = fopen(POLKIT_RULE_DST, "w");
	if (action && rule) {
		fprintf(action, ACTION_TEMPLATE, exe);
        fputs(RULE_TEMPLATE, rule);
		ret = VANILLA_SUCCESS;
	}

	if (action)	fclose(action);
	if (rule) fclose(rule);

	return ret;
}

void pipe_listen(int local, const char *wireless_interface, const char *log_file)
{
    // Some setups require this
#ifndef ANDROID
    run_process_and_read_stdout((const char *[]) {"rfkill", "unblock", "wlan", NULL}, 0, 0);
#endif

    // Store reference to log file
    ext_logfile = log_file;

#ifdef ANDROID
    if (vanilla_android_acquire_wifi(wireless_interface) < 0)
        return;
#endif

    int skt = open_socket(local, VANILLA_PIPE_CMD_SERVER_PORT);
    if (skt == -1) {
        nlprint("Failed to open server socket");
#ifdef ANDROID
        vanilla_android_release_wifi();
#endif
        return;
    }

    vanilla_pipe_command_t cmd;

    struct sockaddr_in addr;

    pthread_mutex_init(&running_mutex, NULL);
    pthread_mutex_init(&action_mutex, NULL);
    pthread_mutex_init(&main_loop_mutex, NULL);

	pthread_t stdin_thread;
	pthread_create(&stdin_thread, NULL, read_stdin, NULL);

	set_signals();

    main_loop = 1;

    pthread_mutex_lock(&main_loop_mutex);

    nlprint("READY");

    // If this is the Steam Deck, we must switch the backend from `iwd` to `wpa_supplicant`
#ifndef ANDROID
	int sd_wifi_backend_changed = 0;
	{
		char sd_wifi_backend_buf[100] = {0};
		ssize_t ret = run_process_and_read_stdout((const char *[]) {"steamos-wifi-set-backend", "--check", NULL}, sd_wifi_backend_buf, sizeof(sd_wifi_backend_buf));
		if (ret > 0 && !strcmp("iwd\n", sd_wifi_backend_buf)) {
			nlprint("STEAM DECK: SETTING WIFI BACKEND TO WPA_SUPPLICANT");
			sd_wifi_backend_changed = 1;
			run_process_and_read_stdout((const char *[]) {"steamos-wifi-set-backend", "wpa_supplicant", NULL}, 0, 0);
		}
	}
#endif

#ifdef USE_LIBNM
    // Check status of interface with NetworkManager
    GError *nm_err;
    NMClient *nmcli = nm_client_new(NULL, &nm_err);
    NMDevice *nmdev = NULL;
    gboolean is_managed = 0;
    if (nmcli) {
        nmdev = nm_client_get_device_by_iface(nmcli, wireless_interface);
        if (!nmdev) {
            nlprint("FAILED TO GET STATUS OF DEVICE %s", wireless_interface);
        } else {
			if ((is_managed = nm_device_get_managed(nmdev))) {
				nm_device_set_managed(nmdev, FALSE);
				nlprint("TEMPORARILY SET %s TO UNMANAGED", wireless_interface);
			}
		}
    } else {
        // Failed to get NetworkManager, host may just not have it?
        g_message("Failed to create NetworkManager client: %s", nm_err->message);
        g_error_free(nm_err);
    }
#endif

    // Disable power saving because the Wii U seems to hate that
    struct nl_sock *ps_nl = NULL;
    int ps_prev_state = -1;
    int ps_ifindex = if_nametoindex(wireless_interface);
    if (ps_ifindex) {
        ps_nl = nl_socket_alloc();
        if (ps_nl && nl_connect(ps_nl, NETLINK_GENERIC) == 0) {
            int nl80211_id = genl_ctrl_resolve(ps_nl, "nl80211");
            if (nl80211_id < 0) {
                nlprint("FAILED TO RESOLVE NL80211 FAMILY: %i", nl80211_id);
            } else {
                nl80211_get_power_save(ps_nl, nl80211_id, ps_ifindex, &ps_prev_state);
                if (ps_prev_state != NL80211_PS_DISABLED) {
                    if (nl80211_set_power_save(ps_nl, nl80211_id, ps_ifindex, NL80211_PS_DISABLED) == 0) {
                        nlprint("TEMPORARILY DISABLED WI-FI POWER SAVING ON %s", wireless_interface);
                    } else {
                        nlprint("FAILED TO DISABLE WI-FI POWER SAVING ON %s", wireless_interface);
                    }
                }
            }
        } else {
            nlprint("FAILED TO CONNECT TO NETLINK FOR POWER SAVE");
        }
    }

    while (main_loop) {
        pthread_mutex_unlock(&main_loop_mutex);

        socklen_t addr_size = sizeof(addr);
        ssize_t r = recvfrom(skt, &cmd, sizeof(cmd), 0, (struct sockaddr *) &addr, &addr_size);
        // ssize_t r = read(skt, &cmd, sizeof(cmd));
        if (r <= 0) {
            goto repeat_loop;
        }

        if (cmd.control_code == VANILLA_PIPE_CC_SYNC || cmd.control_code == VANILLA_PIPE_CC_CONNECT) {
            if (pthread_mutex_trylock(&action_mutex) == 0) {
                struct sync_args *args = malloc(sizeof(struct sync_args));
                args->wireless_interface = wireless_interface;
                args->local = local;
                args->skt = skt;
                args->client = addr;

                if (cmd.control_code == VANILLA_PIPE_CC_SYNC) {
                    args->code = ntohs(cmd.sync.code);
                    args->start_routine = vanilla_sync_with_console;
                } else {
                    memcpy(args->bssid, cmd.connection.bssid.bssid, sizeof(cmd.connection.bssid.bssid));
                    memcpy(args->psk, cmd.connection.psk.psk, sizeof(cmd.connection.psk.psk));
                    args->start_routine = vanilla_connect_to_console;
                }

                // Acknowledge
                cmd.control_code = VANILLA_PIPE_CC_BIND_ACK;
                if (sendto(skt, &cmd, sizeof(cmd.control_code), 0, (const struct sockaddr *) &addr, addr_size) == -1) {
                    nlprint("FAILED TO SEND ACK: %i", errno);
                }

                pthread_t thread;
                int thread_ret;
                if (pthread_create(&thread, NULL, thread_handler, args) != 0) {
                    nlprint("FAILED TO CREATE THREAD");
                    free(args);
                }
            } else {
                // Busy
                cmd.control_code = VANILLA_PIPE_CC_BUSY;
                if (sendto(skt, &cmd, sizeof(cmd.control_code), 0, (const struct sockaddr *) &addr, addr_size) == -1) {
                    nlprint("FAILED TO SEND BUSY: %i", errno);
                }
            }
		} else if (cmd.control_code == VANILLA_PIPE_CC_INSTALL_POLKIT || cmd.control_code == VANILLA_PIPE_CC_UNINSTALL_POLKIT) {
			if (cmd.control_code == VANILLA_PIPE_CC_INSTALL_POLKIT) {
				// Write Polkit rule and action
				install_polkit();
			} else {
				// Delete Polkit rule and action
				uninstall_polkit();
			}

			// Acknowledge
			cmd.control_code = VANILLA_PIPE_CC_BIND_ACK;
			if (sendto(skt, &cmd, sizeof(cmd.control_code), 0, (const struct sockaddr *) &addr, addr_size) == -1) {
				nlprint("FAILED TO SEND ACK: %i", errno);
			}
        } else if (cmd.control_code == VANILLA_PIPE_CC_UNBIND) {
            nlprint("RECEIVED UNBIND SIGNAL");
            interrupt();
        } else if (cmd.control_code == VANILLA_PIPE_CC_QUIT) {
            quit_loop();
        }

repeat_loop:
        pthread_mutex_lock(&main_loop_mutex);
    }

    pthread_mutex_unlock(&main_loop_mutex);

    // Wait for any potential running actions to complete
    pthread_mutex_lock(&action_mutex);
    pthread_mutex_unlock(&action_mutex);

	// Wait for stdin thread
	pthread_join(stdin_thread, NULL);

    pthread_mutex_destroy(&main_loop_mutex);
    pthread_mutex_destroy(&action_mutex);
    pthread_mutex_destroy(&running_mutex);

die_and_reenable_managed:
#ifdef USE_LIBNM
    if (is_managed) {
        nlprint("SETTING %s BACK TO MANAGED", wireless_interface);
        nm_device_set_managed(nmdev, TRUE);
    }

die_and_close_nmcli:
    if (nmcli) {
        g_object_unref(nmcli);
    }
#endif

#ifndef ANDROID
	if (sd_wifi_backend_changed) {
		// Restore iwd
		nlprint("STEAM DECK: SETTING WIFI BACKEND TO IWD");
		run_process_and_read_stdout((const char *[]) {"steamos-wifi-set-backend", "iwd", NULL}, 0, 0);
	}
#endif

	// Restore previous Wi-Fi power saving state
	if (ps_nl) {
		if (ps_prev_state != -1 && ps_prev_state != NL80211_PS_DISABLED) {
			int nl80211_id = genl_ctrl_resolve(ps_nl, "nl80211");
			if (nl80211_id >= 0 && nl80211_set_power_save(ps_nl, nl80211_id, ps_ifindex, ps_prev_state) == 0) {
				nlprint("RESTORED WI-FI POWER SAVING ON %s", wireless_interface);
			} else {
				nlprint("FAILED TO RESTORE WI-FI POWER SAVING ON %s", wireless_interface);
			}
		}
		nl_close(ps_nl);
		nl_socket_free(ps_nl);
	}

#ifdef ANDROID
    vanilla_android_release_wifi();
#endif
}

int vanilla_has_config()
{
    return (access(get_wireless_connect_config_filename(), F_OK) == 0);
}
