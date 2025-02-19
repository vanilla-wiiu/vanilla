/* dhcpc.c
 *
 * udhcp DHCP client
 *
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sys/time.h>
#include <sys/file.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>

#include "dhcpd.h"
#include "dhcpc.h"
#include "options.h"
#include "clientpacket.h"
#include "clientsocket.h"
#include "script.h"
#include "socket.h"
#include "common.h"
#include "signalpipe.h"

static int state;
static unsigned long requested_ip; /* = 0 */
static unsigned long server_addr;
static unsigned long timeout;
static int packet_num; /* = 0 */
static int fd = -1;

#define LISTEN_NONE 0
#define LISTEN_KERNEL 1
#define LISTEN_RAW 2
static int listen_mode;

struct client_config_t client_config = {
	/* Default options. */
	abort_if_no_lease: 0,
	foreground: 0,
	quit_after_lease: 0,
	background_if_no_lease: 0,
	interface: "eth0",
	pidfile: NULL,
	callback: NULL,
	clientid: NULL,
	hostname: NULL,
	fqdn: NULL,
	ifindex: 0,
	arp: "\0\0\0\0\0\0",		/* appease gcc-3.0 */
};

#ifndef IN_BUSYBOX
static void __attribute__ ((noreturn)) show_usage(void)
{
	printf(
"Usage: udhcpc [OPTIONS]\n\n"
"  -c, --clientid=CLIENTID         Client identifier\n"
"  -H, --hostname=HOSTNAME         Client hostname\n"
"  -h                              Alias for -H\n"
"  -F, --fqdn=FQDN                 Client fully qualified domain name\n"
"  -f, --foreground                Do not fork after getting lease\n"
"  -b, --background                Fork to background if lease cannot be\n"
"                                  immediately negotiated.\n"
"  -i, --interface=INTERFACE       Interface to use (default: eth0)\n"
"  -n, --now                       Exit with failure if lease cannot be\n"
"                                  immediately negotiated.\n"
"  -p, --pidfile=file              Store process ID of daemon in file\n"
"  -q, --quit                      Quit after obtaining lease\n"
"  -r, --request=IP                IP address to request (default: none)\n"
"  -s, --script=file               Run file at dhcp events (default:\n"
"                                  " DEFAULT_SCRIPT ")\n"
"  -v, --version                   Display version\n"
	);
	exit(0);
}
#else
#define show_usage bb_show_usage
extern void show_usage(void) __attribute__ ((noreturn));
#endif


/* just a little helper */
static void change_mode(int new_mode)
{
	DEBUG(LOG_INFO, "entering %s listen mode",
		new_mode ? (new_mode == 1 ? "kernel" : "raw") : "none");
	if (fd >= 0) close(fd);
	fd = -1;
	listen_mode = new_mode;
}


/* perform a renew */
static void perform_renew(void)
{
	LOG(LOG_INFO, "Performing a DHCP renew");
	switch (state) {
	case BOUND:
		change_mode(LISTEN_KERNEL);
	case RENEWING:
	case REBINDING:
		state = RENEW_REQUESTED;
		break;
	case RENEW_REQUESTED: /* impatient are we? fine, square 1 */
		run_script(NULL, "deconfig");
	case REQUESTING:
	case RELEASED:
		change_mode(LISTEN_RAW);
		state = INIT_SELECTING;
		break;
	case INIT_SELECTING:
		break;
	}

	/* start things over */
	packet_num = 0;

	/* Kill any timeouts because the user wants this to hurry along */
	timeout = 0;
}


/* perform a release */
static void perform_release(void)
{
	char buffer[16];
	struct in_addr temp_addr;

	/* send release packet */
	if (state == BOUND || state == RENEWING || state == REBINDING) {
		temp_addr.s_addr = server_addr;
		sprintf(buffer, "%s", inet_ntoa(temp_addr));
		temp_addr.s_addr = requested_ip;
		LOG(LOG_INFO, "Unicasting a release of %s to %s",
				inet_ntoa(temp_addr), buffer);
		send_release(server_addr, requested_ip); /* unicast */
		run_script(NULL, "deconfig");
	}
	LOG(LOG_INFO, "Entering released state");

	change_mode(LISTEN_NONE);
	state = RELEASED;
	timeout = 0x7fffffff;
}


static void client_background(void)
{
	background(client_config.pidfile);
	client_config.foreground = 1; /* Do not fork again. */
	client_config.background_if_no_lease = 0;
}


int udhcpc_main()
{
	uint8_t *temp, *message;
	unsigned long t1 = 0, t2 = 0, xid = 0;
	unsigned long start = 0, lease;
	fd_set rfds;
	int retval;
	struct timeval tv;
	int c, len;
	struct dhcpMessage packet;
	struct in_addr temp_addr;
	long now;
	int max_fd;
	int sig;

	static const struct option arg_options[] = {
		{"clientid",	required_argument,	0, 'c'},
		{"foreground",	no_argument,		0, 'f'},
		{"background",	no_argument,		0, 'b'},
		{"hostname",	required_argument,	0, 'H'},
		{"hostname",	required_argument,	0, 'h'},
		{"fqdn",	required_argument,	0, 'F'},
		{"interface",	required_argument,	0, 'i'},
		{"now", 	no_argument,		0, 'n'},
		{"pidfile",	required_argument,	0, 'p'},
		{"quit",	no_argument,		0, 'q'},
		{"request",	required_argument,	0, 'r'},
		{"script",	required_argument,	0, 's'},
		{"version",	no_argument,		0, 'v'},
		{0, 0, 0, 0}
	};

	/* get options */
	// while (1) {
	// 	int option_index = 0;
	// 	c = getopt_long(argc, argv, "c:fbH:h:F:i:np:qr:s:v", arg_options, &option_index);
	// 	if (c == -1) break;

	// 	switch (c) {
	// 	case 'c':
	// 		len = strlen(optarg) > 255 ? 255 : strlen(optarg);
	// 		if (client_config.clientid) free(client_config.clientid);
	// 		client_config.clientid = xmalloc(len + 2);
	// 		client_config.clientid[OPT_CODE] = DHCP_CLIENT_ID;
	// 		client_config.clientid[OPT_LEN] = len;
	// 		client_config.clientid[OPT_DATA] = '\0';
	// 		strncpy(client_config.clientid + OPT_DATA, optarg, len);
	// 		break;
	// 	case 'f':
	// 		client_config.foreground = 1;
	// 		break;
	// 	case 'b':
	// 		client_config.background_if_no_lease = 1;
	// 		break;
	// 	case 'h':
	// 	case 'H':
	// 		len = strlen(optarg) > 255 ? 255 : strlen(optarg);
	// 		if (client_config.hostname) free(client_config.hostname);
	// 		client_config.hostname = xmalloc(len + 2);
	// 		client_config.hostname[OPT_CODE] = DHCP_HOST_NAME;
	// 		client_config.hostname[OPT_LEN] = len;
	// 		strncpy(client_config.hostname + 2, optarg, len);
	// 		break;
	// 	case 'F':
	// 		len = strlen(optarg) > 255 ? 255 : strlen(optarg);
	// 		if (client_config.fqdn) free(client_config.fqdn);
	// 		client_config.fqdn = xmalloc(len + 5);
	// 		client_config.fqdn[OPT_CODE] = DHCP_FQDN;
	// 		client_config.fqdn[OPT_LEN] = len + 3;
	// 		/* Flags: 0000NEOS
	// 		S: 1 => Client requests Server to update A RR in DNS as well as PTR
	// 		O: 1 => Server indicates to client that DNS has been updated regardless
	// 		E: 1 => Name data is DNS format, i.e. <4>host<6>domain<4>com<0> not "host.domain.com"
	// 		N: 1 => Client requests Server to not update DNS
	// 		*/
	// 		client_config.fqdn[OPT_LEN + 1] = 0x1;
	// 		client_config.fqdn[OPT_LEN + 2] = 0;
	// 		client_config.fqdn[OPT_LEN + 3] = 0;
	// 		strncpy(client_config.fqdn + 5, optarg, len);
	// 		break;
	// 	case 'i':
	// 		client_config.interface =  optarg;
	// 		break;
	// 	case 'n':
	// 		client_config.abort_if_no_lease = 1;
	// 		break;
	// 	case 'p':
	// 		client_config.pidfile = optarg;
	// 		break;
	// 	case 'q':
	// 		client_config.quit_after_lease = 1;
	// 		break;
	// 	case 'r':
	// 		requested_ip = inet_addr(optarg);
	// 		break;
	// 	case 's':
	// 		client_config.script = optarg;
	// 		break;
	// 	case 'v':
	// 		printf("udhcpcd, version %s\n\n", VERSION);
	// 		return 0;
	// 		break;
	// 	default:
	// 		show_usage();
	// 	}
	// }

	/* Start the log, sanitize fd's, and write a pid file */
	start_log_and_pid("udhcpc", client_config.pidfile);

	if (read_interface(client_config.interface, &client_config.ifindex,
			   NULL, client_config.arp) < 0)
		return 1;

	if (!client_config.clientid) {
		client_config.clientid = xmalloc(6 + 3);
		client_config.clientid[OPT_CODE] = DHCP_CLIENT_ID;
		client_config.clientid[OPT_LEN] = 7;
		client_config.clientid[OPT_DATA] = 1;
		memcpy(client_config.clientid + 3, client_config.arp, 6);
	}

	/* setup the signal pipe */
	udhcp_sp_setup();

	state = INIT_SELECTING;
	run_script(NULL, "deconfig");
	change_mode(LISTEN_RAW);

	timeout = 0;

	for (;;) {

		tv.tv_sec = timeout - uptime();
		tv.tv_usec = 0;

		if (listen_mode != LISTEN_NONE && fd < 0) {
			if (listen_mode == LISTEN_KERNEL)
				fd = listen_socket(INADDR_ANY, CLIENT_PORT, client_config.interface);
			else
				fd = raw_socket(client_config.ifindex);
			if (fd < 0) {
				LOG(LOG_ERR, "FATAL: couldn't listen on socket, %m");
				return 0;
			}
		}
		max_fd = udhcp_sp_fd_set(&rfds, fd);

		if (tv.tv_sec > 0) {
			DEBUG(LOG_INFO, "Waiting on select...");
			retval = select(max_fd + 1, &rfds, NULL, NULL, &tv);
		} else retval = 0; /* If we already timed out, fall through */

		now = uptime();
		if (retval == 0) {
			/* timeout dropped to zero */
			switch (state) {
			case INIT_SELECTING:
				if (packet_num < 3) {
					if (packet_num == 0)
						xid = random_xid();

					/* send discover packet */
					send_discover(xid, requested_ip); /* broadcast */

					timeout = now + ((packet_num == 2) ? 4 : 2);
					packet_num++;
				} else {
					run_script(NULL, "leasefail");
					if (client_config.background_if_no_lease) {
						LOG(LOG_INFO, "No lease, forking to background.");
						client_background();
					} else if (client_config.abort_if_no_lease) {
						LOG(LOG_INFO, "No lease, failing.");
						return 1;
				  	}
					/* wait to try again */
					packet_num = 0;
					timeout = now + 60;
				}
				break;
			case RENEW_REQUESTED:
			case REQUESTING:
				if (packet_num < 3) {
					/* send request packet */
					if (state == RENEW_REQUESTED)
						send_renew(xid, server_addr, requested_ip); /* unicast */
					else send_selecting(xid, server_addr, requested_ip); /* broadcast */

					timeout = now + ((packet_num == 2) ? 10 : 2);
					packet_num++;
				} else {
					/* timed out, go back to init state */
					if (state == RENEW_REQUESTED) run_script(NULL, "deconfig");
					state = INIT_SELECTING;
					timeout = now;
					packet_num = 0;
					change_mode(LISTEN_RAW);
				}
				break;
			case BOUND:
				/* Lease is starting to run out, time to enter renewing state */
				state = RENEWING;
				change_mode(LISTEN_KERNEL);
				DEBUG(LOG_INFO, "Entering renew state");
				/* fall right through */
			case RENEWING:
				/* Either set a new T1, or enter REBINDING state */
				if ((t2 - t1) <= (lease / 14400 + 1)) {
					/* timed out, enter rebinding state */
					state = REBINDING;
					timeout = now + (t2 - t1);
					DEBUG(LOG_INFO, "Entering rebinding state");
				} else {
					/* send a request packet */
					send_renew(xid, server_addr, requested_ip); /* unicast */

					t1 = (t2 - t1) / 2 + t1;
					timeout = t1 + start;
				}
				break;
			case REBINDING:
				/* Either set a new T2, or enter INIT state */
				if ((lease - t2) <= (lease / 14400 + 1)) {
					/* timed out, enter init state */
					state = INIT_SELECTING;
					LOG(LOG_INFO, "Lease lost, entering init state");
					run_script(NULL, "deconfig");
					timeout = now;
					packet_num = 0;
					change_mode(LISTEN_RAW);
				} else {
					/* send a request packet */
					send_renew(xid, 0, requested_ip); /* broadcast */

					t2 = (lease - t2) / 2 + t2;
					timeout = t2 + start;
				}
				break;
			case RELEASED:
				/* yah, I know, *you* say it would never happen */
				timeout = 0x7fffffff;
				break;
			}
		} else if (retval > 0 && listen_mode != LISTEN_NONE && FD_ISSET(fd, &rfds)) {
			/* a packet is ready, read it */

			if (listen_mode == LISTEN_KERNEL)
				len = get_packet(&packet, fd);
			else len = get_raw_packet(&packet, fd);

			if (len == -1 && errno != EINTR) {
				DEBUG(LOG_INFO, "error on read, %m, reopening socket");
				change_mode(listen_mode); /* just close and reopen */
			}
			if (len < 0) continue;

			if (packet.xid != xid) {
				DEBUG(LOG_INFO, "Ignoring XID %lx (our xid is %lx)",
					(unsigned long) packet.xid, xid);
				continue;
			}

			if ((message = get_option(&packet, DHCP_MESSAGE_TYPE)) == NULL) {
				DEBUG(LOG_ERR, "couldnt get option from packet -- ignoring");
				continue;
			}

			switch (state) {
			case INIT_SELECTING:
				/* Must be a DHCPOFFER to one of our xid's */
				if (*message == DHCPOFFER) {
					if ((temp = get_option(&packet, DHCP_SERVER_ID))) {
						memcpy(&server_addr, temp, 4);
						xid = packet.xid;
						requested_ip = packet.yiaddr;

						/* enter requesting state */
						state = REQUESTING;
						timeout = now;
						packet_num = 0;
					} else {
						DEBUG(LOG_ERR, "No server ID in message");
					}
				}
				break;
			case RENEW_REQUESTED:
			case REQUESTING:
			case RENEWING:
			case REBINDING:
				if (*message == DHCPACK) {
					if (!(temp = get_option(&packet, DHCP_LEASE_TIME))) {
						LOG(LOG_ERR, "No lease time with ACK, using 1 hour lease");
						lease = 60 * 60;
					} else {
						memcpy(&lease, temp, 4);
						lease = ntohl(lease);
					}

					/* enter bound state */
					t1 = lease / 2;

					/* little fixed point for n * .875 */
					t2 = (lease * 0x7) >> 3;
					temp_addr.s_addr = packet.yiaddr;
					LOG(LOG_INFO, "Lease of %s obtained, lease time %ld",
						inet_ntoa(temp_addr), lease);
					start = now;
					timeout = t1 + start;
					requested_ip = packet.yiaddr;
					run_script(&packet,
						   ((state == RENEWING || state == REBINDING) ? "renew" : "bound"));

					state = BOUND;
					change_mode(LISTEN_NONE);
					if (client_config.quit_after_lease)
						return 0;
					if (!client_config.foreground)
						client_background();

				} else if (*message == DHCPNAK) {
					/* return to init state */
					LOG(LOG_INFO, "Received DHCP NAK");
					run_script(&packet, "nak");
					if (state != REQUESTING)
						run_script(NULL, "deconfig");
					state = INIT_SELECTING;
					timeout = now;
					requested_ip = 0;
					packet_num = 0;
					change_mode(LISTEN_RAW);
					sleep(3); /* avoid excessive network traffic */
				}
				break;
			/* case BOUND, RELEASED: - ignore all packets */
			}
		} else if (retval > 0 && (sig = udhcp_sp_read(&rfds))) {
			switch (sig) {
			case SIGUSR1:
				perform_renew();
				break;
			case SIGUSR2:
				perform_release();
				break;
			case SIGTERM:
				LOG(LOG_INFO, "Received SIGTERM");
				return 0;
			}
		} else if (retval == -1 && errno == EINTR) {
			/* a signal was caught */
		} else {
			/* An error occured */
			DEBUG(LOG_ERR, "Error on select");
		}

	}
	return 0;
}
