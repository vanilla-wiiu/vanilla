#ifndef VANILLA_PIPE_DEF_H
#define VANILLA_PIPE_DEF_H

#define VANILLA_PIPE_CMD_PORT 51000

#define VANILLA_PIPE_CC_SYNC 0x80
#define VANILLA_PIPE_CC_CONNECT 0x81
#define VANILLA_PIPE_CC_BIND_ACK 0x82
#define VANILLA_PIPE_CC_STATUS 0x83
#define VANILLA_PIPE_CC_PING 0x84
#define VANILLA_PIPE_CC_BUSY 0x85
#define VANILLA_PIPE_CC_UNBIND 0x86
#define VANILLA_PIPE_CC_SYNC_SUCCESS 0x87

#define VANILLA_PIPE_LOCAL_SOCKET "/tmp/vanilla-pipe.%i"

#pragma pack(push, 1)
typedef struct {
    uint16_t code;
} vanilla_pipe_sync_info_t;

typedef struct {
    int32_t status;
} vanilla_pipe_status_info_t;

typedef struct {
    unsigned char bssid[6];
    unsigned char psk[32];
} vanilla_pipe_connection_info_t;

typedef struct {
    uint8_t control_code;
    union{
        vanilla_pipe_sync_info_t sync;
        vanilla_pipe_connection_info_t connection;
        vanilla_pipe_status_info_t status;
    };
} vanilla_pipe_command_t;
#pragma pack(pop)

#endif // VANILLA_PIPE_DEF_H