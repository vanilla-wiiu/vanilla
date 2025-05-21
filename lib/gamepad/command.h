#ifndef GAMEPAD_COMMAND_H
#define GAMEPAD_COMMAND_H

#include <stddef.h>
#include <stdint.h>

typedef struct
{
    // Little endian
    uint16_t packet_type;
    uint16_t query_type;
    uint16_t payload_size;
    uint16_t seq_id;
} CmdHeader;
static_assert(sizeof(CmdHeader) == 0x8);

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
    uint8_t ids[3];
    uint8_t flags;
    uint8_t service_id;
    uint8_t method_id;
    uint16_t error_code;            // Big endian
    uint16_t payload_size;          // Big endian
} GenericCmdHeader;
static_assert(sizeof(GenericCmdHeader) == 0xC);

typedef struct
{
    CmdHeader cmd_header;
    GenericCmdHeader generic_cmd_header;
    uint8_t payload[1544];
} GenericPacket;
static_assert(offsetof(GenericPacket, generic_cmd_header) == 0x8);
static_assert(sizeof(GenericPacket) == 0x61C);

struct UvcUacCommand {
    uint8_t f1;
    uint16_t unknown_0;
    uint8_t f3;
    uint8_t mic_enable;
    uint8_t mic_mute;
    int16_t mic_volume;
    int16_t mic_volume_2;
    uint8_t unknown_A;
    uint8_t unknown_B;
    uint16_t mic_freq;
    uint8_t cam_enable;
    uint8_t cam_power;
    uint8_t cam_power_freq;
    uint8_t cam_auto_expo;
    uint32_t cam_expo_absolute;
    uint16_t cam_brightness;
    uint16_t cam_contrast;
    uint16_t cam_gain;
    uint16_t cam_hue;
    uint16_t cam_saturation;
    uint16_t cam_sharpness;
    uint16_t cam_gamma;
    uint8_t cam_key_frame;
    uint8_t cam_white_balance_auto;
    uint32_t cam_white_balance;
    uint16_t cam_multiplier;
    uint16_t cam_multiplier_limit;
};

struct TimeCommand {
    uint16_t days_counter;
    uint16_t padding;
    uint32_t seconds_counter;
};

typedef struct
{
    CmdHeader cmd_header;
    struct UvcUacCommand uac_uvc;
} UvcUacPacket;

typedef struct
{
    CmdHeader cmd_header;
    struct TimeCommand time;
} TimePacket;

typedef struct
{
    uint16_t new_min_x;
    uint16_t new_min_y;
    uint16_t new_max_x;
    uint16_t new_max_y;
    uint16_t old_min_x;
    uint16_t old_min_y;
    uint16_t old_max_x;
    uint16_t old_max_y;
    uint16_t crc;
} TouchPadCalibration;

typedef struct
{
    uint8_t unk00[0x103];
    uint8_t region;
    uint16_t region_crc;
    uint8_t unk11e[0x4d];
    TouchPadCalibration factory_touchpad_calibration;
    uint8_t unk19e[0x6e];
    TouchPadCalibration second_factory_touchpad_calibration;
    uint8_t unk200[0x5f];
    TouchPadCalibration touchpad_calibration;
    uint8_t unk256[0xaa];
} EEPROM;
static_assert(offsetof(EEPROM, region) == 0x103);
static_assert(offsetof(EEPROM, factory_touchpad_calibration) == 0x153);
static_assert(offsetof(EEPROM, second_factory_touchpad_calibration) == 0x1D3);
static_assert(offsetof(EEPROM, touchpad_calibration) == 0x244);
static_assert(sizeof(EEPROM) == 0x300);

enum BoardVersion
{
    BOARD_VERSION_DK1,
    BOARD_VERSION_DK1_EP_DK2,
    BOARD_VERSION_DP1,
    BOARD_VERSION_DP2,
    BOARD_VERSION_DK3,
    BOARD_VERSION_DK4,
    BOARD_VERSION_PreDP3_DP3,
    BOARD_VERSION_DK5,
    BOARD_VERSION_DP4,
    BOARD_VERSION_DKMP,
    BOARD_VERSION_DP5,
    BOARD_VERSION_MASS,
    BOARD_VERSION_DKMP2,
    BOARD_VERSION_DRC_I,
    BOARD_VERSION_DKTVMP,
};

enum ChipVersion
{
    CHIP_VERSION_TS = 0x10,
    CHIP_VERSION_ES1 = 0x20,
    CHIP_VERSION_ES2 = 0x30,
    CHIP_VERSION_ES3 = 0x40,
    CHIP_VERSION_MS01 = 0x41,
};

typedef struct
{
    uint32_t board_ver;
    uint32_t chip_ver;
    uint32_t run_ver;
    uint32_t uic_ver;
    uint32_t unk10;
    uint32_t sd_cis;
    uint32_t spl_id;
} SystemInfo;
static_assert(sizeof(SystemInfo) == 0x1C);
#pragma pack(pop)

enum CommandType
{
    CMD_GENERIC,
    CMD_UVC_UAC,
    CMD_TIME
};

enum ServiceID
{
    SERVICE_ID_SOFTWARE,
    SERVICE_ID_CONFIG,
    SERVICE_ID_ADMIN,
    SERVICE_ID_METRICS,
    SERVICE_ID_SYSTEM,
    SERVICE_ID_PERIPHERAL
};

enum MethodIDSoftware
{
    METHOD_ID_SOFTWARE_GET_VERSION = 0x0,
    METHOD_ID_SOFTWARE_GET_EXT_ID = 0xa,
};

enum MethodIDSystem
{
    METHOD_ID_SYSTEM_GET_INFO = 0x4,
    METHOD_ID_SYSTEM_POWER = 0x1A,
};

enum MethodIDPeripheral
{
    METHOD_ID_PERIPHERAL_EEPROM = 0x6,
    METHOD_ID_PERIPHERAL_UPDATE_EEPROM = 0xC,
    METHOD_ID_PERIPHERAL_SET_REMOCON = 0x18,
};

void *listen_command(void *x);

void set_region(int region);

CmdHeader create_ack_packet(CmdHeader *pkt);
void send_ack_packet(int skt, CmdHeader *pkt);

#endif // GAMEPAD_COMMAND_H
