#include "command.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "gamepad.h"
#include "vanilla.h"
#include "util.h"

const char *uic_firmware_version = "\x28\x00\x00\x58";
const char *eeprom_bytes = "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\x80\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x40\x51\x32\x00\x02\x06\xd3\x36\x21\x31\x60\x52\x50\x64\xcb\xe7\x47\x0c\xca\x8a\x7e\x79\xf3\xb4\x70\xea\x34\xaf\x2c\xa0\x4b\xc6\x70\x49\x01\x0e\x1e\x24\xa1\x68\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\x1c\x3d\x8b\x5c\x35\x01\x0e\x1e\x15\xab\x48\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xb8\xf0\x06\xff\x10\xab\xbc\xff\x20\x00\x11\xff\x6f\x1f\x61\x1f\x55\x1e\x60\x52\xc5\x00\x00\xf0\xff\xff\x09\x00\x00\x3a\x5c\x02\x32\x57\x02\xd0\x5b\x02\xc8\xd2\x66\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x35\x00\x1e\x00\x22\x03\xc3\x01\x53\x01\x5d\x0e\xa9\x0e\x9b\x01\x66\xae\xe4\x17\x03\xa0\xb5\x43\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x8b\x5c\x35\x01\x0e\x1e\x15\xab\x48\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xb8\xf0\x06\xff\x10\xab\xbc\xff\x20\x00\x11\xff\x6f\x1f\x61\x1f\x55\x1e\x60\x52\xc5\x00\x00\xf0\xff\xff\x09\x00\x00\x3a\x5c\x02\x32\x57\x02\xd0\x5b\x02\xc8\xd2\x66\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x35\x00\x1e\x00\x22\x03\xc3\x01\x53\x01\x5d\x0e\xa9\x0e\x9b\x01\x66\xae\xe4\x17\x03\xa0\xb5\x43\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x05\x2a\x58\x00\x87\x0f\x00\x87\x0f\x01\x0e\x1e\x00\x00\x00\x00\x00\x19\x00\x16\x1d\x6f\xbc\xff\x20\x00\x11\xff\x6f\x1f\x61\x1f\x55\x1e\x60\x52\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x5d\x00\x78\x00\xf8\x02\x82\x01\xfc\x01\x92\x0b\xf1\x0d\x6c\x03\x48\x1a\x01\x00\x02\x17\x14\x48\x00\x00\x00\x03\xba\x31\xe4\x17\x03\xa0\xb5\x43\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x87\x0f\x15\x01\x2d\x01\x4d\x01\x7a\x01\xb7\x01\xff\x01\x03\x26\x8c\x04\xa3\x49\x54\x31\x39\x36\x33\x53\x31\x33\x37\x37\x78\x01\x00\x87\x0f\x00\x87\x0f\x01\x0e\x1e\xff\xff\x00\x00\x87\x0f\x00\x87\x0f\x02\x00\x08\xc3\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04\xa3\x49";

int current_region = VANILLA_REGION_AMERICA;
void set_region(int region)
{
    current_region = region;
}

CmdHeader create_ack_packet(CmdHeader *pkt)
{
	CmdHeader ack;
    ack.packet_type = pkt->packet_type == PACKET_TYPE_REQUEST ? PACKET_TYPE_REQUEST_ACK : PACKET_TYPE_RESPONSE_ACK;
    ack.query_type = pkt->query_type;
    ack.payload_size = 0;
    ack.seq_id = pkt->seq_id;
	return ack;
}

void send_ack_packet(int skt, CmdHeader *pkt)
{
	CmdHeader ack = create_ack_packet(pkt);
    send_to_console(skt, &ack, sizeof(ack), PORT_CMD);
}

void send_quick_response(int skt, CmdHeader *request)
{
    CmdHeader response;
    response.packet_type = PACKET_TYPE_RESPONSE;
    response.payload_size = 0;
    response.query_type = request->query_type;
    response.seq_id = request->seq_id;
    send_to_console(skt, &response, sizeof(CmdHeader), PORT_CMD);
}

void send_generic_response(int skt, CmdHeader *response)
{
    response->packet_type = PACKET_TYPE_RESPONSE;

    send_to_console(skt, response, response->payload_size + sizeof(CmdHeader), PORT_CMD);
}

void handle_generic_packet(gamepad_context_t *info, int skt, GenericPacket *request)
{
    GenericCmdHeader *gen_cmd = &request->generic_cmd_header;
    vanilla_log("magic: %x, flags: %x, service ID: %u, method ID: %u", gen_cmd->magic_0x7E, gen_cmd->flags, gen_cmd->service_id, gen_cmd->method_id);

    // Prepare response
    GenericPacket response;
    memset(&response, 0, sizeof(response));
    memcpy(&response.generic_cmd_header, &request->generic_cmd_header, sizeof(GenericCmdHeader));
    response.generic_cmd_header.ids[2] = 0;
    uint8_t req_flags = request->generic_cmd_header.flags;
    response.generic_cmd_header.flags = (uint8_t)((req_flags & 0x3C) << 3) | ((req_flags >> 0x3) & 0xFC) | 1;
    response.generic_cmd_header.error_code = htons(1);
    response.generic_cmd_header.payload_size = htons(0);

    switch (gen_cmd->service_id) {
    case SERVICE_ID_SOFTWARE:
        switch (gen_cmd->method_id) {
        case METHOD_ID_SOFTWARE_GET_VERSION: {
            uint32_t run_ver, act_ver;
            run_ver = act_ver = 0x190c0117;

            response.payload[0] = (uint8_t)(run_ver >> 24);
            response.payload[1] = (uint8_t)(run_ver >> 16);
            response.payload[2] = (uint8_t)(run_ver >> 8);
            response.payload[3] = (uint8_t)run_ver;
            response.payload[4] = (uint8_t)(act_ver >> 24);
            response.payload[5] = (uint8_t)(act_ver >> 16);
            response.payload[6] = (uint8_t)(act_ver >> 8);
            response.payload[7] = (uint8_t)act_ver;
            response.generic_cmd_header.payload_size = htons(8);
            response.generic_cmd_header.error_code = 0;
            break;
        }
        case METHOD_ID_SOFTWARE_GET_EXT_ID: {
            // TODO
            uint32_t extId = 0;
            if (request->payload[0] == 0) {
                extId = 0x4170200;
            }
            response.payload[0] = (uint8_t)(extId >> 24);
            response.payload[1] = (uint8_t)(extId >> 16);
            response.payload[2] = (uint8_t)(extId >> 8);
            response.payload[3] = (uint8_t)extId;
            response.generic_cmd_header.payload_size = htons(4);
            response.generic_cmd_header.error_code = 0;
            break;
        }
        }
        break;
    case SERVICE_ID_SYSTEM:
        switch (gen_cmd->method_id) {
        case METHOD_ID_SYSTEM_GET_INFO:
        {
            SystemInfo *info = (SystemInfo *)&response.payload[0];
            info->board_ver = htonl(BOARD_VERSION_MASS);
            info->chip_ver = htonl(CHIP_VERSION_ES3);
            info->run_ver = htonl(0x190c0117);
            info->uic_ver = htonl(0x28000058); // TODO check
            info->unk10 = htonl(0x80000);
            info->sd_cis = htonl(0x00c0221f);
            info->spl_id = htonl(0x00001c58);
            response.generic_cmd_header.payload_size = htons(sizeof(SystemInfo));
            response.generic_cmd_header.error_code = 0;
            break;
        }
        case METHOD_ID_SYSTEM_POWER:
        {
            if (gen_cmd->flags == 0x42) {
                // Console told us to poweroff
                int err = VANILLA_ERR_SHUTDOWN;
                push_event(info->event_loop, VANILLA_EVENT_ERROR, &err, sizeof(int));
            }
            break;
        }
        case 10:
            response.generic_cmd_header.error_code = 0;
            break;
        case 11:
            response.payload[0] = 0;
            response.generic_cmd_header.payload_size = htons(1);
            response.generic_cmd_header.error_code = 0;
            break;
        }
        break;
    case SERVICE_ID_PERIPHERAL:
        switch (gen_cmd->method_id) {
        case METHOD_ID_PERIPHERAL_EEPROM:
        {
            memcpy(&response.payload[0], uic_firmware_version, 4);
            memcpy(&response.payload[4], eeprom_bytes, 768);

            EEPROM *e = (EEPROM *)&response.payload[4];

            e->region = current_region;
            e->region_crc = crc16(&e->region, sizeof(e->region));

            e->touchpad_calibration.new_min_x = htons(0);
            e->touchpad_calibration.new_max_x = htons(854);
            e->touchpad_calibration.new_min_y = htons(0);
            e->touchpad_calibration.new_max_y = htons(480);
            e->touchpad_calibration.old_min_x = htons(0);
            e->touchpad_calibration.old_max_x = htons(4096);
            e->touchpad_calibration.old_min_y = htons(0);
            e->touchpad_calibration.old_max_y = htons(4096);
            e->touchpad_calibration.crc = crc16((const char *)&e->touchpad_calibration, sizeof(e->touchpad_calibration) - 2);

            response.generic_cmd_header.payload_size = htons(sizeof(EEPROM) + 4);
            response.generic_cmd_header.error_code = 0;
            break;
        }
        case METHOD_ID_PERIPHERAL_UPDATE_EEPROM:
        {
            vanilla_log("5,12 - index: %u, length: %u", response.payload[0], response.payload[1]);
            response.generic_cmd_header.error_code = 0;
            break;
        }
        case METHOD_ID_PERIPHERAL_SET_REMOCON:
        {
            vanilla_log("5,24 - str1: %s, str2: %s", response.payload, response.payload + 5);
            response.generic_cmd_header.error_code = 0;
            break;
        }
        }
        break;
    }

    // send response
    response.cmd_header.seq_id = request->cmd_header.seq_id;
    response.cmd_header.query_type = request->cmd_header.query_type;
    response.cmd_header.payload_size = ntohs(response.generic_cmd_header.payload_size) + sizeof(GenericCmdHeader);
    send_generic_response(skt, (CmdHeader *) &response);
}

void handle_uac_uvc_packet(gamepad_context_t *info, int skt, UvcUacPacket *request)
{
    vanilla_log("uac/uvc - mic_enable: %u, mic_freq: %u, mic_mute: %u, mic_volume: %i, mic_volume2: %i", request->uac_uvc.mic_enable, request->uac_uvc.mic_freq, request->uac_uvc.mic_mute, request->uac_uvc.mic_volume, request->uac_uvc.mic_volume_2);

	uint8_t mic_enabled = request->uac_uvc.mic_enable;
	push_event(info->event_loop, VANILLA_EVENT_MIC, &mic_enabled, sizeof(mic_enabled));

	print_hex(&request->uac_uvc, sizeof(request->uac_uvc));
	vanilla_log_no_newline("\n");

	// TODO: Create real response instead of using canned data
	static const size_t uvc_resp_size = 16;
	const char *uvc_resp = "\x00\x80\x00\x80\x00\x80\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00";
	const char *uvc_resp2 = "\x00\x16\x00\x19\x9E\x00\x00\x00\x80\x00\x40\x00\x00\x00\x00\xFF";
	char buf[sizeof(CmdHeader) + uvc_resp_size];

	CmdHeader *response = (CmdHeader *) buf;
    response->packet_type = PACKET_TYPE_RESPONSE;
    response->payload_size = uvc_resp_size;
    response->query_type = request->cmd_header.query_type;
    response->seq_id = request->cmd_header.seq_id;

	memcpy(buf + sizeof(CmdHeader), uvc_resp, uvc_resp_size);

    send_to_console(skt, buf, sizeof(buf), PORT_CMD);
}

void handle_time_packet(int skt, TimePacket *request)
{
    vanilla_log("time - days: %u, padding: %u, seconds: %u", request->time.days_counter, request->time.padding, request->time.seconds_counter);

    send_quick_response(skt, &request->cmd_header);
}

void handle_command_packet(gamepad_context_t *info, int skt, CmdHeader *request)
{
	vanilla_log("packet_type: %u, query_type: %u, payload_size: 0x%X", request->packet_type, request->query_type, request->payload_size);
    switch (request->packet_type)
    {
    case PACKET_TYPE_REQUEST:
        send_ack_packet(skt, request);
        switch (request->query_type)
        {
        case CMD_GENERIC:
        {
            handle_generic_packet(info, skt, (GenericPacket *)request);
            break;
        }
        case CMD_UVC_UAC:
        {
            handle_uac_uvc_packet(info, skt, (UvcUacPacket *)request);
            break;
        }
        case CMD_TIME:
        {
            handle_time_packet(skt, (TimePacket *)request);
            break;
        }
        default:
            vanilla_log("[Command] Unhandled request command: %u", request->query_type);
        }
        break;
    case PACKET_TYPE_RESPONSE:
        send_ack_packet(skt, request);
        switch (request->query_type)
        {
        default:
            // vanilla_log("[Command] Unhandled request command: %u", request->query_type);
            break;
        }
        break;
    case PACKET_TYPE_REQUEST_ACK:
    case PACKET_TYPE_RESPONSE_ACK:
        // NOTE: Should we do something if we don't get this? e.g. attempt re-sending request/response after some time has passed
		//		 Real gamepad attempts 5 times in 33ms intervals if ACK is not received
        break;
    default:
        vanilla_log("Unhandled command packet type: %u", request->packet_type);
    }
}

void *listen_command(void *x)
{
    gamepad_context_t *info = (gamepad_context_t *)x;

    unsigned char data[sizeof(CmdHeader) + 2048];
    ssize_t size;

    do
    {
        size = recv(info->socket_cmd, data, sizeof(data), 0);
        if (size > 0) {
            CmdHeader *header = (CmdHeader *)data;
            handle_command_packet(info, info->socket_cmd, header);
        }
    } while (!is_interrupted());

    pthread_exit(NULL);

    return NULL;
}
