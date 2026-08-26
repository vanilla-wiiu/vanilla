#ifndef GAMEPAD_NFC_H
#define GAMEPAD_NFC_H

#include "vanilla.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

enum NfcCommand {
    NFC_COMMAND_STARTUP = 0x00,
    NFC_COMMAND_READ_START = 0x01,
    NFC_COMMAND_READ = 0x02,
    NFC_COMMAND_WRITE_START = 0x03,
    NFC_COMMAND_RESULT_CHECK = 0x04,
    NFC_COMMAND_ABORT = 0x05,
    NFC_COMMAND_SHUTDOWN = 0x06,
    NFC_COMMAND_FORMAT_START = 0x07,
    NFC_COMMAND_SET_READ_ONLY = 0x08,
    NFC_COMMAND_IS_TAG_PRESENT = 0x09,
    NFC_COMMAND_PASS_THROUGH_SEND = 0x0A,
    NFC_COMMAND_PASS_THROUGH_RECEIVE = 0x0B,
    NFC_COMMAND_SET_MODE = 0x0C,
    NFC_COMMAND_DETECT_START = 0x0D,
    NFC_COMMAND_DETECT = 0x0E,
    NFC_COMMAND_DETECT_START_MULTI = 0x0F,
    NFC_COMMAND_DETECT_MULTI = 0x10,
    NFC_COMMAND_PASS_THROUGH_SEND2 = 0x11,
    NFC_COMMAND_PASS_THROUGH_RECEIVE2 = 0x12,
    NFC_COMMAND_ANTENNA_CHECK = 0x13,
    NFC_COMMAND_READ_T2T_START = 0x14,
    NFC_COMMAND_READ_T2T = 0x15,
    NFC_COMMAND_WRITE_T2T = 0x16,
};

enum NfcProtocol {
    NFC_PROTOCOL_UNKNOWN = 0x00,
    NFC_PROTOCOL_T1T = 0x01,
    NFC_PROTOCOL_T2T = 0x02,
    NFC_PROTOCOL_T3T = 0x03,
    NFC_PROTOCOL_ISO_DEP = 0x04,
    NFC_PROTOCOL_15693 = 0x83,
};

enum NfcTechnology {
    NFC_TECHNOLOGY_A = 0x00,
    NFC_TECHNOLOGY_B = 0x01,
    NFC_TECHNOLOGY_F = 0x02,
    NFC_TECHNOLOGY_ISO15693 = 0x06,
};

enum NFCTechnologyMask {
    NFC_TECHNOLOGY_MASK_ALL = 0,
    NFC_TECHNOLOGY_MASK_A = (1u << 0),
    NFC_TECHNOLOGY_MASK_B = (1u << 1),
    NFC_TECHNOLOGY_MASK_F = (1u << 2),
    NFC_TECHNOLOGY_MASK_ISO15693 = (1u << 3),
};

#pragma pack(push, 1)
typedef struct {
    uint8_t power_mode;
} NfcStartupRequest;
static_assert(sizeof(NfcStartupRequest) == 0x1);

typedef struct {
    uint8_t start_discovery;
    uint16_t discovery_timeout;
    uint32_t command_timeout;
    uint16_t command_size;
    uint16_t response_size;
    uint8_t command_data[0x200];
} NfcPassThroughSendRequest;
static_assert(sizeof(NfcPassThroughSendRequest) == 0x20B);

typedef struct {
    uint8_t mode;
} NfcSetModeRequest;
static_assert(sizeof(NfcSetModeRequest) == 0x1);

typedef struct {
    uint8_t start;
    uint8_t end;
} NfcReadT2TRange;
static_assert(sizeof(NfcReadT2TRange) == 0x02);

typedef struct {
    uint16_t discovery_timeout;
    uint8_t uid[7];
    uint8_t uid_mask[7];
    uint8_t expected_version[8];
    uint32_t command_timeout;
    uint8_t num_ranges;
    NfcReadT2TRange ranges[4];
    uint8_t pwd_auth;
    uint8_t padding[0x19];
} NfcReadT2TStartRequest;
static_assert(sizeof(NfcReadT2TStartRequest) == 0x3F);

typedef struct {
    uint8_t address;
    uint8_t size;
    uint8_t data[0xF0];
} NfcWriteT2TRange;
static_assert(sizeof(NfcWriteT2TRange) == 0xF2);

typedef struct {
    uint16_t discovery_timeout;
    uint8_t uid[7];
    uint8_t uid_mask[7];
    uint8_t version[8];
    uint32_t command_timeout;
    uint8_t num_ranges;
    NfcWriteT2TRange ranges[4];
    uint8_t activation_address;
    uint8_t deactivation_data[4];
    uint8_t activation_data[4];
    uint8_t pwd_auth;
    uint8_t activation;
    uint8_t padding[0xF];
} NfcWriteT2TRequest;
static_assert(sizeof(NfcWriteT2TRequest) == 0x3FF);

typedef struct {
    uint8_t command;
    union {
        NfcStartupRequest startup;
        NfcPassThroughSendRequest pass_through_send;
        NfcSetModeRequest set_mode;
        NfcReadT2TStartRequest read_t2t_start;
        NfcWriteT2TRequest write_t2t;
    };
} NfcRequest;

typedef struct {
    uint8_t uid[7];
} NfcResultCheckResponse;
static_assert(sizeof(NfcResultCheckResponse) == 0x7);

typedef struct {
    uint16_t responseSize;
    uint8_t data[0x200];
} NfcPassThroughReceiveReponse;
static_assert(sizeof(NfcPassThroughReceiveReponse) == 0x202);

typedef struct {
    uint8_t rf_disc_id;
    uint8_t protocol;
    uint8_t discovery_type;
    uint8_t uid_size;
    uint8_t uid[10];
    uint8_t version[8];
    uint8_t padding[0x10];
    uint8_t num_ranges;
    NfcReadT2TRange ranges[4];
    uint8_t data[0x3A0];
    uint8_t signature[0x20];
} NfcReadT2TResponse;
static_assert(sizeof(NfcReadT2TResponse) == 0x3EF);

typedef struct {
    uint8_t result;
    union {
        NfcResultCheckResponse result_check;
        NfcPassThroughReceiveReponse pass_through_receive;
        NfcReadT2TResponse read_t2t;
    };
} NfcResponse;
#pragma pack(pop)

void *gamepad_nfc_process_commands(void *arg);

void gamepad_nfc_set_backend(const VanillaNfcBackend *backend);

void gamepad_nfc_control(const uint8_t *request_bytes, uint16_t request_size, uint8_t *response_bytes,
                         uint16_t *response_size);

int gamepad_nfc_init(void);

void gamepad_nfc_deinit(void);

#endif // GAMEPAD_NFC_H
