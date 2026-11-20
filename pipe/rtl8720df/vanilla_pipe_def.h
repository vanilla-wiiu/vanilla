#ifndef VANILLA_PIPE_DEF_H
#define VANILLA_PIPE_DEF_H

#include <stdint.h>
#include <stddef.h>

#define VANILLA_PIPE_CMD_SERVER_PORT 51000
#define VANILLA_PIPE_CMD_CLIENT_PORT 51100

#define VANILLA_PIPE_CC_SYNC         0x80
#define VANILLA_PIPE_CC_CONNECT      0x81
#define VANILLA_PIPE_CC_BIND_ACK     0x82
#define VANILLA_PIPE_CC_STATUS       0x83
#define VANILLA_PIPE_CC_PING         0x84
#define VANILLA_PIPE_CC_BUSY         0x85
#define VANILLA_PIPE_CC_UNBIND       0x86
#define VANILLA_PIPE_CC_SYNC_SUCCESS 0x87
#define VANILLA_PIPE_CC_CONNECTED    0x88
#define VANILLA_PIPE_CC_DISCONNECTED 0x89

#define VANILLA_ERR_GENERIC (-1)
#define VANILLA_ERR_UNKNOWN_COMMAND (-2)

#pragma pack(push, 1)

typedef struct {
    unsigned char bssid[6];
} vanilla_bssid_t;

typedef struct {
    unsigned char psk[32];
} vanilla_psk_t;

typedef struct {
    vanilla_bssid_t bssid;
    vanilla_psk_t psk;
} vanilla_connection_t;

typedef struct {
    uint16_t code;
} vanilla_pipe_sync_info_t;

typedef struct {
    int32_t status;
} vanilla_pipe_status_info_t;

typedef struct {
    uint8_t control_code;

    union {
        vanilla_pipe_sync_info_t sync;
        vanilla_connection_t connection;
        vanilla_pipe_status_info_t status;
    };
} vanilla_pipe_command_t;

#pragma pack(pop)

static_assert(sizeof(vanilla_connection_t) == 38, "Incorrect connection size");

static_assert(sizeof(vanilla_pipe_command_t) == 39, "Incorrect command size");

static_assert(offsetof(vanilla_pipe_command_t, sync) == 1, "Incorrect payload offset");

static_assert(sizeof(vanilla_pipe_sync_info_t) == 2, "Incorrect sync size");

static_assert(sizeof(vanilla_pipe_status_info_t) == 4, "Incorrect status size");

#endif // VANILLA_PIPE_DEF_H