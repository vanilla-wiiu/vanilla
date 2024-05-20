#include "input.h"

#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "gamepad.h"
#include "vanilla.h"
#include "util.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define CLAMP(x, min, max) (MIN(MAX(x, min), max))

#pragma pack(push, 1)

typedef struct {
    // Little endian
    int16_t x;
    int16_t y;
    int16_t z;
} InputPacketAccelerometerWiiU;

typedef struct {
    // Little endian
    signed roll : 24;
    signed yaw : 24;
    signed pitch : 24;
} InputPacketGyroscopeWiiU;

typedef struct {
    signed char unknown[6];
} InputPacketMagnetWiiU;

typedef struct {
    unsigned pad : 1;
    unsigned extra : 3;
    unsigned value : 12;
} TouchCoord;

typedef struct {
    TouchCoord x;
    TouchCoord y;
} TouchPoint;

typedef struct {
    // Big endian
    TouchPoint points[10];
} TouchScreenState;

pthread_mutex_t button_mtx;
uint16_t current_buttons = 0;
uint8_t current_extra_buttons = 0;
float current_stick_left_x = 0;
float current_stick_left_y = 0;
float current_stick_right_x = 0;
float current_stick_right_y = 0;
int current_touch_x = -1;
int current_touch_y = -1;

typedef struct {
    // Big endian
    uint16_t seq_id;
    uint16_t buttons;
    uint8_t power_status;
    uint8_t battery_charge;
    uint16_t stick_left_x;
    uint16_t stick_left_y;
    uint16_t stick_right_x;
    uint16_t stick_right_y;
    uint8_t audio_volume;
    InputPacketAccelerometerWiiU accelerometer;
    InputPacketGyroscopeWiiU gyroscope;
    InputPacketMagnetWiiU magnet;
    TouchScreenState touchscreen; // byte 36 - 76
    unsigned char unknown_0[4];
    uint8_t extra_buttons;
    unsigned char unknown_1[46];
    uint8_t fw_version_neg;
} InputPacket;

#pragma pack(pop)

void set_button_state_bit(uint16_t bit_flag, int pressed)
{
    pthread_mutex_lock(&button_mtx);
    if (pressed) {
        current_buttons |= bit_flag;
    } else {
        current_buttons &= ~bit_flag;
    }
    pthread_mutex_unlock(&button_mtx);
}

void set_extra_button_state_bit(uint8_t bit_flag, int pressed)
{
    pthread_mutex_lock(&button_mtx);
    if (pressed) {
        current_extra_buttons |= bit_flag;
    } else {
        current_extra_buttons &= ~bit_flag;
    }
    pthread_mutex_unlock(&button_mtx);
}

void set_button_state(int button_id, int pressed)
{
    switch (button_id) {
    case VANILLA_BTN_A:
        set_button_state_bit(0x8000, pressed);
        break;
    case VANILLA_BTN_B:
        set_button_state_bit(0x4000, pressed);
        break;
    case VANILLA_BTN_X:
        set_button_state_bit(0x2000, pressed);
        break;
    case VANILLA_BTN_Y:
        set_button_state_bit(0x1000, pressed);
        break;
    case VANILLA_BTN_L:
        set_button_state_bit(0x0020, pressed);
        break;
    case VANILLA_BTN_R:
        set_button_state_bit(0x0010, pressed);
        break;
    case VANILLA_BTN_ZL:
        set_button_state_bit(0x0080, pressed);
        break;
    case VANILLA_BTN_ZR:
        set_button_state_bit(0x0040, pressed);
        break;
    case VANILLA_BTN_MINUS:
        set_button_state_bit(0x0004, pressed);
        break;
    case VANILLA_BTN_PLUS:
        set_button_state_bit(0x0008, pressed);
        break;
    case VANILLA_BTN_HOME:
        set_button_state_bit(0x0002, pressed);
        break;
    case VANILLA_BTN_LEFT:
        set_button_state_bit(0x800, pressed);
        break;
    case VANILLA_BTN_RIGHT:
        set_button_state_bit(0x400, pressed);
        break;
    case VANILLA_BTN_DOWN:
        set_button_state_bit(0x100, pressed);
        break;
    case VANILLA_BTN_UP:
        set_button_state_bit(0x200, pressed);
        break;

    // "Extra" buttons
    case VANILLA_BTN_L3:
        set_extra_button_state_bit(0x80, pressed);
        break;
    case VANILLA_BTN_R3:
        set_extra_button_state_bit(0x40, pressed);
        break;
    }
}

void set_axis_state(int axis_id, float value)
{
    pthread_mutex_lock(&button_mtx);
    switch (axis_id) {
    case VANILLA_AXIS_L_X:
        current_stick_left_x = value;
        break;
    case VANILLA_AXIS_L_Y:
        current_stick_left_y = value;
        break;
    case VANILLA_AXIS_R_X:
        current_stick_right_x = value;
        break;
    case VANILLA_AXIS_R_Y:
        current_stick_right_y = value;
        break;
    }
    pthread_mutex_unlock(&button_mtx);
}

void set_touch_state(int x, int y)
{
    pthread_mutex_lock(&button_mtx);
    current_touch_x = x;
    current_touch_y = y;
    pthread_mutex_unlock(&button_mtx);
}

uint16_t transform_axis_value(float real)
{
    return ((int) (real * 1024)) + 2048;
}

int64_t scale_x_touch_value(uint16_t val)
{
    int64_t v = val;

    // Scales 0-854 to 0-4096 with a 2.5% margin on each side
    const int scale_percent = 95;

    v *= 4096;
    v *= scale_percent;
    v /= 854;
    v /= 100;
    v += (4096 * (100 - scale_percent) / 200);

    return v;
}

int64_t scale_y_touch_value(uint16_t val)
{
    int64_t v = val;

    // Scales 0-854 to 0-4096 with a 5% margin on the bottom and 3% margin on the top (I don't know why, but these values worked best)
    const int scale_percent = 92;

    v *= 4096;
    v *= scale_percent;
    v /= 480;
    v /= 100;
    v += (4096 * (100 - 90) / 200);
    v = 4096 - v;

    return v;
}

void send_input(int socket_hid)
{
    InputPacket ip;
    memset(&ip, 0, sizeof(ip));

    static uint16_t seq_id = 0;

    pthread_mutex_lock(&button_mtx);

    if (current_touch_x >= 0 && current_touch_y >= 0) {
        for (int i = 0; i < 10; i++) {
            ip.touchscreen.points[i].x.pad = 1;
            ip.touchscreen.points[i].y.pad = 1;
            ip.touchscreen.points[i].x.value = reverse_bits(scale_x_touch_value(current_touch_x), 12);
            ip.touchscreen.points[i].y.value = reverse_bits(scale_y_touch_value(current_touch_y), 12);
        }
        ip.touchscreen.points[0].x.extra = 0;
        ip.touchscreen.points[0].y.extra = reverse_bits(2, 3);
        ip.touchscreen.points[1].x.extra = reverse_bits(7, 3);
        ip.touchscreen.points[1].y.extra = reverse_bits(3, 3);
        for (int byte = 0; byte < sizeof(ip.touchscreen); byte += 2) {
            unsigned char *touchscreen_bytes = (unsigned char *) (&ip.touchscreen);
            unsigned char first = (unsigned char) reverse_bits(touchscreen_bytes[byte], 8);
            touchscreen_bytes[byte] = (unsigned char) reverse_bits(touchscreen_bytes[byte + 1], 8);
            touchscreen_bytes[byte + 1] = first;
        }
    }

    ip.buttons = htons(current_buttons);
    ip.extra_buttons = current_extra_buttons;

    ip.stick_left_x = transform_axis_value(current_stick_left_x);
    ip.stick_left_y = transform_axis_value(-current_stick_left_y);
    ip.stick_right_x = transform_axis_value(current_stick_right_x);
    ip.stick_right_y = transform_axis_value(-current_stick_right_y);

    pthread_mutex_unlock(&button_mtx);

    ip.seq_id = htons(seq_id);
    seq_id++;

    ip.fw_version_neg = 215;

    send_to_console(socket_hid, &ip, sizeof(ip), PORT_HID);
}

void *listen_input(void *x)
{
    struct gamepad_thread_context *info = (struct gamepad_thread_context *) x;

    pthread_mutex_init(&button_mtx, NULL);

    do {
        send_input(info->socket_hid);
        usleep(10 * 1000); // Produces 100Hz input, probably no need to go higher for the Wii U, but potentially could
    } while (!is_interrupted());

    pthread_mutex_destroy(&button_mtx);

    pthread_exit(NULL);
    
    return NULL;
}