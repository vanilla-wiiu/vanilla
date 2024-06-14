#include "command.h"

#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "gamepad.h"
#include "status.h"
#include "vanilla.h"
#include "util.h"

typedef struct
{
    // Little endian
    uint16_t packet_type;
    uint16_t query_type;
    uint16_t payload_size;
    uint16_t seq_id;
    uint8_t payload[2048];
} CmdHeader;

enum PacketType
{
    PACKET_TYPE_REQUEST,
    PACKET_TYPE_REQUEST_ACK,
    PACKET_TYPE_RESPONSE,
    PACKET_TYPE_RESPONSE_ACK
};

#pragma pack(push, 1)
typedef struct
{
    uint8_t magic_0x7E;
    uint8_t version;
    unsigned transaction_id : 12;   // Big endian
    unsigned fragment_id : 12;      // Big endian
    uint8_t flags;
    uint8_t service_id;
    uint8_t method_id;
    uint16_t error_code;            // Big endian
    uint16_t payload_size;          // Big endian
} GenericCmdHeader;

typedef struct
{
    uint16_t new_min_x;
    uint16_t new_max_y;
    uint16_t new_max_x;
    uint16_t new_min_y;
    uint16_t old_min_x;
    uint16_t old_max_y;
    uint16_t old_max_x;
    uint16_t old_min_y;
    uint16_t crc;
} TouchPadCalibration;

typedef struct
{
    uint8_t unk00[0x153];
    TouchPadCalibration factory_touchpad_calibration;
    uint8_t unk19e[0xe];
    TouchPadCalibration second_factory_touchpad_calibration;
    uint8_t unk200[0x44];
    TouchPadCalibration touchpad_calibration;
    uint8_t unk256[0xAA];
} EEPROM;
#pragma pack(pop)

const char *uic_firmware_version = "\x28\x00\x00\x58";
const char *eeprom_bytes = "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\x80\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x40\x51\x32\x00\x02\x06\xd3\x36\x21\x31\x60\x52\x50\x64\xcb\xe7\x47\x0c\xca\x8a\x7e\x79\xf3\xb4\x70\xea\x34\xaf\x2c\xa0\x4b\xc6\x70\x49\x01\x0e\x1e\x24\xa1\x68\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\x1c\x3d\x8b\x5c\x35\x01\x0e\x1e\x15\xab\x48\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xb8\xf0\x06\xff\x10\xab\xbc\xff\x20\x00\x11\xff\x6f\x1f\x61\x1f\x55\x1e\x60\x52\xc5\x00\x00\xf0\xff\xff\x09\x00\x00\x3a\x5c\x02\x32\x57\x02\xd0\x5b\x02\xc8\xd2\x66\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x35\x00\x1e\x00\x22\x03\xc3\x01\x53\x01\x5d\x0e\xa9\x0e\x9b\x01\x66\xae\xe4\x17\x03\xa0\xb5\x43\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x8b\x5c\x35\x01\x0e\x1e\x15\xab\x48\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xb8\xf0\x06\xff\x10\xab\xbc\xff\x20\x00\x11\xff\x6f\x1f\x61\x1f\x55\x1e\x60\x52\xc5\x00\x00\xf0\xff\xff\x09\x00\x00\x3a\x5c\x02\x32\x57\x02\xd0\x5b\x02\xc8\xd2\x66\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x35\x00\x1e\x00\x22\x03\xc3\x01\x53\x01\x5d\x0e\xa9\x0e\x9b\x01\x66\xae\xe4\x17\x03\xa0\xb5\x43\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x05\x2a\x58\x00\x87\x0f\x00\x87\x0f\x01\x0e\x1e\x00\x00\x00\x00\x00\x19\x00\x16\x1d\x6f\xbc\xff\x20\x00\x11\xff\x6f\x1f\x61\x1f\x55\x1e\x60\x52\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x5d\x00\x78\x00\xf8\x02\x82\x01\xfc\x01\x92\x0b\xf1\x0d\x6c\x03\x48\x1a\x01\x00\x02\x17\x14\x48\x00\x00\x00\x03\xba\x31\xe4\x17\x03\xa0\xb5\x43\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x87\x0f\x15\x01\x2d\x01\x4d\x01\x7a\x01\xb7\x01\xff\x01\x03\x26\x8c\x04\xa3\x49\x54\x31\x39\x36\x33\x53\x31\x33\x37\x37\x78\x01\x00\x87\x0f\x00\x87\x0f\x01\x0e\x1e\xff\xff\x00\x00\x87\x0f\x00\x87\x0f\x02\x00\x08\xc3\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04\xa3\x49";

enum CommandType {
    CMD_GENERIC,
    CMD_UVC_UAC,
    CMD_TIME
};

enum ServiceID
{
    SERVICE_ID_FIRMWARE,
    SERVICE_ID_WIFI_KEYS,
    SERVICE_ID_2,
    SERVICE_ID_WIFI_STATS,
    SERVICE_ID_4,
    SERVICE_ID_MISC
};

enum MethodIDMisc
{
    METHOD_ID_MISC_EEPROM = 0x6
};

void send_ack_packet(int skt, CmdHeader *pkt)
{
    CmdHeader ack;
    ack.packet_type = pkt->packet_type == PACKET_TYPE_REQUEST ? PACKET_TYPE_REQUEST_ACK : PACKET_TYPE_RESPONSE_ACK;
    ack.query_type = pkt->query_type;
    ack.payload_size = 0;
    ack.seq_id = pkt->seq_id;
    send_to_console(skt, &ack, sizeof(ack), PORT_CMD);
}

void send_response(int skt, CmdHeader *request, const char *payload, size_t payload_size)
{
    CmdHeader rsp;
    rsp.packet_type = PACKET_TYPE_RESPONSE;
    rsp.query_type = request->query_type;
    rsp.seq_id = request->seq_id;
    rsp.payload_size = payload_size;

    memcpy(rsp.payload, payload, payload_size);

    send_to_console(skt, &rsp, sizeof(rsp) - sizeof(rsp.payload) + payload_size, PORT_CMD);
}

void handle_command_packet(int skt, CmdHeader *request)
{
    // TODO: Disabled for now
    /*
    switch (request->packet_type)
    {
    case PACKET_TYPE_REQUEST:
        send_ack_packet(skt, request);
        switch (request->query_type)
        {
        case CMD_GENERIC:
        {
            // for (int i = 0; i < header->payload_size; i++) {
            //     vanilla_log_no_newline("%02X", header->payload[i] & 0xFF);
            // }
            // vanilla_log_no_newline("\n");
            GenericCmdHeader *gen_cmd = (GenericCmdHeader *)request->payload;
            // print_info("magic: %x, flags: %x, service ID: %u, method ID: %u", gen_cmd->magic_0x7E, gen_cmd->flags, gen_cmd->service_id, gen_cmd->method_id);

            switch (gen_cmd->service_id) {
            case SERVICE_ID_4:
                switch (gen_cmd->method_id) {
                case 4:
                    
                    break;
                }
                break;
            case SERVICE_ID_MISC:
                switch (gen_cmd->method_id) {
                case METHOD_ID_MISC_EEPROM:
                {
                    char payload[768 + 4];

                    memcpy(payload, uic_firmware_version, 4);
                    memcpy(payload+4, eeprom_bytes, 768);
                    
                    EEPROM *e = (EEPROM *) (payload+4);
                    // e->touchpad_calibration.new_min_x = 0;
                    // e->touchpad_calibration.new_max_x = 854;
                    // e->touchpad_calibration.new_min_y = 0;
                    // e->touchpad_calibration.new_max_y = 480;
                    // e->touchpad_calibration.old_min_x = 0;
                    // e->touchpad_calibration.old_max_x = 4096;
                    // e->touchpad_calibration.old_min_y = 0;
                    // e->touchpad_calibration.old_max_y = 4096;
                    //e->touchpad_calibration.crc = crc16((const char *)&e->touchpad_calibration, sizeof(e->touchpad_calibration) - 2);
                    print_info("factory: old min: (%u, %u) - old max: (%u, %u) - new min: (%u, %u) - new max: (%u, %u) - crc: %x - our crc: %x",
                               e->factory_touchpad_calibration.old_min_x,
                               e->factory_touchpad_calibration.old_min_y,
                               e->factory_touchpad_calibration.old_max_x,
                               e->factory_touchpad_calibration.old_max_y,
                               e->factory_touchpad_calibration.new_min_x,
                               e->factory_touchpad_calibration.new_min_y,
                               e->factory_touchpad_calibration.new_max_x,
                               e->factory_touchpad_calibration.new_max_y,
                               e->factory_touchpad_calibration.crc,
                               crc16(0, &e->factory_touchpad_calibration, sizeof(e->touchpad_calibration) - 2));
                    print_info("2nd factory: old min: (%u, %u) - old max: (%u, %u) - new min: (%u, %u) - new max: (%u, %u) - crc: %x - our crc: %x",
                               e->second_factory_touchpad_calibration.old_min_x,
                               e->second_factory_touchpad_calibration.old_min_y,
                               e->second_factory_touchpad_calibration.old_max_x,
                               e->second_factory_touchpad_calibration.old_max_y,
                               e->second_factory_touchpad_calibration.new_min_x,
                               e->second_factory_touchpad_calibration.new_min_y,
                               e->second_factory_touchpad_calibration.new_max_x,
                               e->second_factory_touchpad_calibration.new_max_y,
                               e->second_factory_touchpad_calibration.crc,
                               crc16(0, &e->second_factory_touchpad_calibration, sizeof(e->touchpad_calibration) - 2));
                    print_info("ours: old min: (%u, %u) - old max: (%u, %u) - new min: (%u, %u) - new max: (%u, %u) - crc: %x - our crc: %x",
                               e->touchpad_calibration.old_min_x,
                               e->touchpad_calibration.old_min_y,
                               e->touchpad_calibration.old_max_x,
                               e->touchpad_calibration.old_max_y,
                               e->touchpad_calibration.new_min_x,
                               e->touchpad_calibration.new_min_y,
                               e->touchpad_calibration.new_max_x,
                               e->touchpad_calibration.new_max_y,
                               e->touchpad_calibration.crc,
                               crc16(0, &e->touchpad_calibration, sizeof(e->touchpad_calibration) - 2));

                    print_info("sending eeprom info");
                    send_response(skt, request, payload, sizeof(payload));
                    break;
                }
                }
                break;
            }

            break;
        }
        default:
            // print_info("[Command] Unhandled request command: %u", request->query_type);
        }
        break;
    case PACKET_TYPE_RESPONSE:
        send_ack_packet(skt, request);
        switch (request->query_type)
        {
        default:
            // print_info("[Command] Unhandled request command: %u", request->query_type);
        }
        break;
    default:
        // print_info("Unhandled command packet type: %u", request->packet_type);
    }
    //print_info("[Command] Type: %u, Command ID: %u, Size: %u\n", pkt->packet_type, pkt->cmd_id, pkt->payload_size);
    */
}

void *listen_command(void *x)
{
    struct gamepad_thread_context *info = (struct gamepad_thread_context *)x;

    unsigned char data[sizeof(CmdHeader)];
    ssize_t size;

    do
    {
        size = recv(info->socket_cmd, data, sizeof(data), 0);
        if (size > 0)
        {
            if (is_stop_code(data, size))
                break;

            CmdHeader *header = (CmdHeader *)data;
            handle_command_packet(info->socket_cmd, header);
        }
    } while (!is_interrupted());

    pthread_exit(NULL);

    return NULL;
}