#ifndef VANILLA_PIPE_H
#define VANILLA_PIPE_H

// Control codes that our pipe will receive
enum pipe_opcodes
{
    VANILLA_PIPE_IN_SYNC = 1,
    VANILLA_PIPE_IN_CONNECT,
    VANILLA_PIPE_IN_BUTTON,
    VANILLA_PIPE_IN_TOUCH,
    VANILLA_PIPE_IN_REQ_IDR,
    VANILLA_PIPE_IN_REGION,
    VANILLA_PIPE_IN_BATTERY,
    VANILLA_PIPE_IN_BIND,

    VANILLA_PIPE_IN_INTERRUPT = 0x1E,
    VANILLA_PIPE_IN_QUIT = 0x1F,

    VANILLA_PIPE_OUT_DATA = 0x20,
    VANILLA_PIPE_OUT_SYNC_STATE,
    VANILLA_PIPE_OUT_BOUND_SUCCESSFUL,
    VANILLA_PIPE_OUT_EOF,

    // Errors that our pipe will send
    VANILLA_PIPE_ERR_SUCCESS = 0x30,
    VANILLA_PIPE_ERR_UNK,
    VANILLA_PIPE_ERR_INVALID,
    VANILLA_PIPE_ERR_BUSY
};

#pragma pack(push, 1)

struct pipe_control_code
{
    uint8_t code;
};

struct pipe_sync_command
{
    struct pipe_control_code base;
    uint16_t code;
};

struct pipe_button_command
{
    struct pipe_control_code base;
    int32_t id;
    int32_t value;
};

struct pipe_touch_command
{
    struct pipe_control_code base;
    int32_t x;
    int32_t y;
};

struct pipe_region_command
{
    struct pipe_control_code base;
    int8_t region;
};

struct pipe_battery_command
{
    struct pipe_control_code base;
    int8_t battery;
};

struct pipe_sync_state_command
{
    struct pipe_control_code base;
    uint8_t state;
};

struct pipe_data_command
{
    struct pipe_control_code base;
    uint8_t event_type;
    uint16_t data_size;
    uint8_t data[UINT16_MAX];
};

#pragma pack(pop)

#endif // VANILLA_PIPE_H