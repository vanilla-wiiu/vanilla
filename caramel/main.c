#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "../lib/gamepad/command.h"
#include "../lib/util.h"
#include "../pipe/ports.h"

static const int PORTS[] = {PORT_VID, PORT_AUD, PORT_HID, PORT_MSG, PORT_CMD};
#define PORT_COUNT 5

typedef struct listener_data_t listener_data_t;
typedef void (*pkt_handler_t)(listener_data_t *ctx, const void *data, size_t size);

static int RUNNING = 1;

typedef struct listener_data_t {
	int socket;
	uint16_t port;
	pthread_t thread;
	int thread_started;
	struct timeval time;
	pkt_handler_t handler;
} listener_data_t;

void nlprint(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
	va_end(va);
}

ssize_t send_to_gamepad(int skt, const void *data, size_t size)
{
	struct sockaddr_in sa = {0};
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("192.168.1.11");
	sa.sin_port = htons(PORT_CMD);

	sendto(skt, data, size, 0, (const struct sockaddr *) &sa, sizeof(sa));
}

int create_socket(uint16_t port)
{
	struct sockaddr_in sa = {0};
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = INADDR_ANY;
	sa.sin_port = htons(port - 100);

	int skt = socket(AF_INET, SOCK_DGRAM, 0);
	if (skt == -1) {
		nlprint("FAILED TO CREATE SOCKET FOR PORT %u: %i", port, errno);
		return -1;
	}

	// Timeout after 250ms
	struct timeval tv = {0};
    tv.tv_usec = 250000;
    setsockopt(skt, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	int ret = bind(skt, (const struct sockaddr *) &sa, sizeof(sa));
	if (ret == -1) {
		nlprint("FAILED TO BIND SOCKET FOR PORT %u: %i", port, errno);
		close(skt);
		return -1;
	}

	return skt;
}

void printstart(listener_data_t *ctx)
{
	struct timeval tv;
	gettimeofday(&tv, 0);

	long time = (tv.tv_sec - ctx->time.tv_sec) * 1000000 + (tv.tv_usec - ctx->time.tv_usec);

	fprintf(stderr, "[%li.%06li] [%u] ", time / 1000000, time % 1000000, ctx->port);
}

void default_handler(listener_data_t *ctx, const void *data, size_t size)
{
	printstart(ctx);
	nlprint("Received packet of size %zi", size);
}

void hid_handler(listener_data_t *ctx, const void *data, size_t size)
{
	static int count = 0;
	const int threshold = 180;
	count++;
	if (count % threshold == 0) {
		// printstart(ctx);
		// nlprint("%i input events", threshold);
	}
}

static pthread_mutex_t cmd_mutex = PTHREAD_MUTEX_INITIALIZER;
static UvcUacPacket uvc_uac_pkt;
static int cmd_seq_id = 0;
void *send_uvc_uac_pkt_loop(void *arg)
{
	listener_data_t *data = (listener_data_t *) arg;

	uvc_uac_pkt.cmd_header.packet_type = PACKET_TYPE_REQUEST;
	uvc_uac_pkt.cmd_header.query_type = CMD_UVC_UAC;
	uvc_uac_pkt.cmd_header.payload_size = sizeof(uvc_uac_pkt.uac_uvc);

	uvc_uac_pkt.uac_uvc.f1 = 1;
	uvc_uac_pkt.uac_uvc.unknown_0 = 0;
	uvc_uac_pkt.uac_uvc.f3 = 0;
	uvc_uac_pkt.uac_uvc.mic_enable = 0;
	uvc_uac_pkt.uac_uvc.mic_mute = 0;
	uvc_uac_pkt.uac_uvc.mic_volume = 0x1600;
	uvc_uac_pkt.uac_uvc.mic_volume_2 = 0x1900;
	uvc_uac_pkt.uac_uvc.unknown_A = 0;
	uvc_uac_pkt.uac_uvc.unknown_B = 0;
	uvc_uac_pkt.uac_uvc.mic_freq = 16000;
	uvc_uac_pkt.uac_uvc.cam_enable = 0;
	uvc_uac_pkt.uac_uvc.cam_power = 0;
	uvc_uac_pkt.uac_uvc.cam_power_freq = 0;
	uvc_uac_pkt.uac_uvc.cam_auto_expo = 1;
	uvc_uac_pkt.uac_uvc.cam_expo_absolute = 0x009E0200;
	uvc_uac_pkt.uac_uvc.cam_brightness = 0;
	uvc_uac_pkt.uac_uvc.cam_contrast = 0;
	uvc_uac_pkt.uac_uvc.cam_gain = 0;
	uvc_uac_pkt.uac_uvc.cam_hue = 0x0070;
	uvc_uac_pkt.uac_uvc.cam_saturation = 0;
	uvc_uac_pkt.uac_uvc.cam_sharpness = 0x4040;
	uvc_uac_pkt.uac_uvc.cam_gamma = 3;
	uvc_uac_pkt.uac_uvc.cam_key_frame = 0x2D;
	uvc_uac_pkt.uac_uvc.cam_white_balance_auto = 0;
	uvc_uac_pkt.uac_uvc.cam_white_balance = 0x00800100;
	uvc_uac_pkt.uac_uvc.cam_multiplier = 0x40;
	uvc_uac_pkt.uac_uvc.cam_multiplier_limit = 0;

	while (RUNNING) {
		pthread_mutex_lock(&cmd_mutex);
		uvc_uac_pkt.cmd_header.seq_id = cmd_seq_id;
		cmd_seq_id++;
		pthread_mutex_unlock(&cmd_mutex);

		nlprint("Sending UAC/UVC packet (seq ID: %X)...", uvc_uac_pkt.cmd_header.seq_id);
		send_to_gamepad(data->socket, &uvc_uac_pkt, sizeof(uvc_uac_pkt));

		sleep(1);
	}
}

void cmd_handler(listener_data_t *ctx, const void *data, size_t size)
{
	static long last_cmd = 0;

	struct timeval tv;
	gettimeofday(&tv, 0);

	long now = tv.tv_sec * 1000000 + tv.tv_usec;
	long delta = now - last_cmd;
	last_cmd = now;

	CmdHeader *hdr = (CmdHeader *) data;
	const char *packet_type_str = "";
	switch (hdr->packet_type) {
	case PACKET_TYPE_REQUEST: packet_type_str = "PACKET_TYPE_REQUEST"; break;
	case PACKET_TYPE_REQUEST_ACK: packet_type_str = "PACKET_TYPE_REQUEST_ACK"; break;
	case PACKET_TYPE_RESPONSE: packet_type_str = "PACKET_TYPE_RESPONSE"; break;
	case PACKET_TYPE_RESPONSE_ACK: packet_type_str = "PACKET_TYPE_RESPONSE_ACK"; break;
	}
	nlprint("[CMD] packet_type = %s, query_type = %X, payload_size = %X, seq_id = %x, delta = %li us", packet_type_str, hdr->query_type, hdr->payload_size, hdr->seq_id, delta);
	if (hdr->payload_size > 0) {
		switch (hdr->query_type) {
		case CMD_GENERIC:
		{
			GenericPacket *gen = (GenericPacket *) data;
			nlprint("[GEN] version: %x, flags: %x, service ID: %u, method ID: %u, error code: %x, ids: %x %x %x, payload_size: %x",
				gen->generic_cmd_header.version,
				gen->generic_cmd_header.flags,
				gen->generic_cmd_header.service_id,
				gen->generic_cmd_header.method_id,
				gen->generic_cmd_header.error_code,
				gen->generic_cmd_header.ids[0],
				gen->generic_cmd_header.ids[1],
				gen->generic_cmd_header.ids[2],
				gen->generic_cmd_header.payload_size);
			break;
		}
		case CMD_UVC_UAC:
		{
			// UvcUacPacket *uvc = (UvcUacPacket *) data;
			// nlprint("[UVC] f1: %x, unknown_0: %x, f3: %x, mic_enable: %x, mic_mute: %x, mic_volume: %x, mic_volume_2: %x, unknown_A: %x, unknown_B: %x, mic_freq: %u",
			// 	uvc->uac_uvc.f1,
			// 	uvc->uac_uvc.unknown_0,
			// 	uvc->uac_uvc.f3,
			// 	uvc->uac_uvc.mic_enable,
			// 	uvc->uac_uvc.mic_mute,
			// 	uvc->uac_uvc.mic_volume,
			// 	uvc->uac_uvc.mic_volume_2,
			// 	uvc->uac_uvc.unknown_A,
			// 	uvc->uac_uvc.unknown_B,
			// 	uvc->uac_uvc.mic_freq);
			print_hex(((const char *) data) + sizeof(CmdHeader), hdr->payload_size);
			printf("\n");
			break;
		}
		default:
		{
			print_hex(((const char *) data) + sizeof(CmdHeader), hdr->payload_size);
			printf("\n");
			break;
		}
		}
	}

	if (hdr->packet_type == PACKET_TYPE_REQUEST || hdr->packet_type == PACKET_TYPE_RESPONSE) {
		CmdHeader ack = create_ack_packet(hdr);
		send_to_gamepad(ctx->socket, &ack, sizeof(ack));
	}
}

void *listen_socket(void *arg)
{
	listener_data_t *data = (listener_data_t *) arg;
	char buf[4096];

	while (RUNNING) {
		ssize_t r = recv(data->socket, buf, sizeof(buf), 0);
		if (r <= 0) {
			continue;
		} else {
			if (data->handler) {
				data->handler(data, buf, r);
			}
		}
	}
}

int main(int argc, const char **argv)
{
	listener_data_t data[PORT_COUNT];

	pthread_t uvc_uac_loop_thread;
	int uvc_uac_loop_thread_created = 0;

	struct timeval start;
	gettimeofday(&start, 0);

	// Set up defaults
	for (int i = 0; i < PORT_COUNT; i++) {
		listener_data_t *d = &data[i];
		d->port = PORTS[i];
		d->socket = -1;
		d->thread_started = 0;
		d->time = start;
		d->handler = default_handler;
	}

	data[2].handler = hid_handler;
	data[4].handler = cmd_handler;

	int ready = 1;

	// Try to set up sockets
	for (int i = 0; i < PORT_COUNT; i++) {
		listener_data_t *d = &data[i];
		int port = d->port;
		int skt = create_socket(port);
		if (skt == -1) {
			ready = 0;
			break;
		}

		d->socket = skt;
	}

	// If all sockets set up, start listening threads
	if (ready) {
		for (int i = 0; i < PORT_COUNT; i++) {
			listener_data_t *d = &data[i];
			if (pthread_create(&d->thread, 0, listen_socket, d) == 0) {
				d->thread_started = 1;
			} else {
				ready = 0;
				break;
			}
		}
	}

	if (ready) {
		// Start ancillary threads
		if (pthread_create(&uvc_uac_loop_thread, 0, send_uvc_uac_pkt_loop, &data[4]) == 0) {
			uvc_uac_loop_thread_created = 1;
		}

		// Wait for close
		nlprint("Running, type 'exit' or 'quit' or 'bye' to leave");
		while (RUNNING) {
			char *d = 0;
			size_t n;
			if (getline(&d, &n, stdin) >= 0) {
				for (size_t i = 0; i < n; i++) {
					if (d[i] == '\n') {
						d[i] = '\0';
						break;
					}
				}
				static uint16_t seq_id = 0;
				if (!strcmp("exit", d) || !strcmp("quit", d) || !strcmp("bye", d)) {
					RUNNING = 0;
				} else if (!strcmp("cmd", d)) {
					GenericPacket pkt;
					pkt.cmd_header.packet_type = PACKET_TYPE_REQUEST;
					pkt.cmd_header.query_type = CMD_GENERIC;
					pkt.cmd_header.payload_size = sizeof(GenericCmdHeader);
					pkt.cmd_header.seq_id = seq_id;
					seq_id++;

					memset(&pkt.generic_cmd_header, 0, sizeof(pkt.generic_cmd_header));
					pkt.generic_cmd_header.magic_0x7E = 0x7E;
					pkt.generic_cmd_header.version = 1;
					pkt.generic_cmd_header.flags = 0x40;
					pkt.generic_cmd_header.service_id = SERVICE_ID_PERIPHERAL;
					pkt.generic_cmd_header.method_id = METHOD_ID_PERIPHERAL_EEPROM;

					nlprint("Sending generic packet (seq ID: %X)...", pkt.cmd_header.seq_id);

					send_to_gamepad(data[4].socket, &pkt, sizeof(CmdHeader) + sizeof(GenericCmdHeader));
				}
			}
			free(d);
		}
	}

	// Join all threads and close all sockets
	RUNNING = 0;
	for (int i = 0; i < PORT_COUNT; i++) {
		listener_data_t *d = &data[i];
		if (d->thread_started) {
			pthread_join(d->thread, 0);
		}

		if (d->socket != -1) {
			close(d->socket);
		}
	}

	if (uvc_uac_loop_thread_created) {
		pthread_join(uvc_uac_loop_thread, 0);
	}

	return 0;
}
