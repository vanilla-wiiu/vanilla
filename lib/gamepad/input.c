#include "input.h"

#include <arpa/inet.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gamepad.h"
#include "status.h"
#include "vanilla.h"
#include "util.h"

#pragma pack(push, 1)

typedef struct {
    // Little endian
    int16_t z;
    int16_t x;
    int16_t y;
} InputPacketAccelerometer;

typedef struct {
    // Little endian
    signed roll : 24;
    signed pitch : 24;
    signed yaw : 24;
} InputPacketGyroscope;

typedef struct {
    signed char unknown[6];
} InputPacketMagnet;

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
int32_t current_buttons[VANILLA_BTN_COUNT] = {0};
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
    InputPacketAccelerometer accelerometer;
    InputPacketGyroscope gyroscope;
    InputPacketMagnet magnet;
    TouchScreenState touchscreen; // byte 36 - 76
    unsigned char unknown_0[4];
    uint8_t extra_buttons;
    unsigned char unknown_1[46];
    uint8_t fw_version_neg;
} InputPacket;

#pragma pack(pop)

void set_button_state(int button, int32_t value)
{
    pthread_mutex_lock(&button_mtx);
    current_buttons[button] = value;
    pthread_mutex_unlock(&button_mtx);
}

void set_touch_state(int x, int y)
{
    pthread_mutex_lock(&button_mtx);
    current_touch_x = x;
    current_touch_y = y;
    pthread_mutex_unlock(&button_mtx);
}

uint16_t resolve_axis_value(float axis, float neg, float pos, int flip)
{
    float val = axis < 0 ? axis / 32768.0f : axis / 32767.0f;

    neg = abs(neg);
    pos = abs(pos);

    neg /= 32767.0f;
    pos /= 32767.0f;

    val -= neg;
    val += pos;

    if (flip) {
        val = -val;
    }
    
    return ((int) (val * 1024)) + 2048;
}

int64_t scale_x_touch_value(int64_t v)
{
    // Scales 0-854 to 0-4096 with a 2.5% margin on each side
    const int scale_percent = 95;

    v *= 4096;
    v *= scale_percent;
    v /= 854;
    v /= 100;
    v += (4096 * (100 - scale_percent) / 200);

    return v;
}

int64_t scale_y_touch_value(int64_t v)
{
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

float unpack_float(int32_t x)
{
    float f;
    memcpy(&f, &x, sizeof(int32_t));
    return f;
}

int current_battery_status = VANILLA_BATTERY_STATUS_CHARGING;
void set_battery_status(int status)
{
    pthread_mutex_lock(&button_mtx);
    current_battery_status = status;
    pthread_mutex_unlock(&button_mtx);
}

void send_input(int socket_hid)
{
    InputPacket ip;
    memset(&ip, 0, sizeof(ip));

    static uint16_t seq_id = 0;

    pthread_mutex_lock(&button_mtx);

    ip.touchscreen.points[9].x.extra = reverse_bits(current_battery_status, 3);

    if (current_touch_x >= 0 && current_touch_y >= 0) {
        for (int i = 0; i < 10; i++) {
            ip.touchscreen.points[i].x.pad = 1;
            ip.touchscreen.points[i].y.pad = 1;
            ip.touchscreen.points[i].x.value = reverse_bits(scale_x_touch_value(current_touch_x), 12);
            ip.touchscreen.points[i].y.value = reverse_bits(scale_y_touch_value(current_touch_y), 12);
        }

        ip.touchscreen.points[0].y.extra = reverse_bits(2, 3);
        ip.touchscreen.points[1].x.extra = reverse_bits(7, 3);
        ip.touchscreen.points[1].y.extra = reverse_bits(3, 3);
    }

    for (int byte = 0; byte < sizeof(ip.touchscreen); byte += 2)
    {
        unsigned char *touchscreen_bytes = (unsigned char *)(&ip.touchscreen);
        unsigned char first = (unsigned char)reverse_bits(touchscreen_bytes[byte], 8);
        touchscreen_bytes[byte] = (unsigned char)reverse_bits(touchscreen_bytes[byte + 1], 8);
        touchscreen_bytes[byte + 1] = first;
    }

    uint16_t button_mask = 0;

    if (current_buttons[VANILLA_BTN_A]) button_mask |= 0x8000;
    if (current_buttons[VANILLA_BTN_B]) button_mask |= 0x4000;
    if (current_buttons[VANILLA_BTN_X]) button_mask |= 0x2000;
    if (current_buttons[VANILLA_BTN_Y]) button_mask |= 0x1000;
    if (current_buttons[VANILLA_BTN_L]) button_mask |= 0x0020;
    if (current_buttons[VANILLA_BTN_R]) button_mask |= 0x0010;
    if (current_buttons[VANILLA_BTN_ZL]) button_mask |= 0x0080;
    if (current_buttons[VANILLA_BTN_ZR]) button_mask |= 0x0040;
    if (current_buttons[VANILLA_BTN_MINUS]) button_mask |= 0x0004;
    if (current_buttons[VANILLA_BTN_PLUS]) button_mask |= 0x0008;
    if (current_buttons[VANILLA_BTN_HOME]) button_mask |= 0x0002;
    if (current_buttons[VANILLA_BTN_LEFT]) button_mask |= 0x800;
    if (current_buttons[VANILLA_BTN_RIGHT]) button_mask |= 0x400;
    if (current_buttons[VANILLA_BTN_DOWN]) button_mask |= 0x100;
    if (current_buttons[VANILLA_BTN_UP]) button_mask |= 0x200;

    ip.buttons = htons(button_mask);

    button_mask = 0;
    
    if (current_buttons[VANILLA_BTN_L3]) button_mask |= 0x80;
    if (current_buttons[VANILLA_BTN_R3]) button_mask |= 0x40;
    if (current_buttons[VANILLA_BTN_TV]) button_mask |= 0x20;

    ip.extra_buttons = button_mask;

    ip.stick_left_x = resolve_axis_value(current_buttons[VANILLA_AXIS_L_X], current_buttons[VANILLA_AXIS_L_LEFT], current_buttons[VANILLA_AXIS_L_RIGHT], 0);
    ip.stick_left_y = resolve_axis_value(current_buttons[VANILLA_AXIS_L_Y], current_buttons[VANILLA_AXIS_L_UP], current_buttons[VANILLA_AXIS_L_DOWN], 1);
    ip.stick_right_x = resolve_axis_value(current_buttons[VANILLA_AXIS_R_X], current_buttons[VANILLA_AXIS_R_LEFT], current_buttons[VANILLA_AXIS_R_RIGHT], 0);
    ip.stick_right_y = resolve_axis_value(current_buttons[VANILLA_AXIS_R_Y], current_buttons[VANILLA_AXIS_R_UP], current_buttons[VANILLA_AXIS_R_DOWN], 1);

    ip.audio_volume = current_buttons[VANILLA_AXIS_VOLUME];

    ip.accelerometer.x = unpack_float(current_buttons[VANILLA_SENSOR_ACCEL_X]) * -800;
    ip.accelerometer.y = unpack_float(current_buttons[VANILLA_SENSOR_ACCEL_Y]) * -800;
    ip.accelerometer.z = unpack_float(current_buttons[VANILLA_SENSOR_ACCEL_Z]) * 800;

    ip.gyroscope.yaw = (unpack_float(current_buttons[VANILLA_SENSOR_GYRO_YAW]) * (180.0f/M_PI)) / ((200.0f * 6.0f) / 154000.0f);
    ip.gyroscope.pitch = (unpack_float(current_buttons[VANILLA_SENSOR_GYRO_PITCH]) * (180.0f/M_PI)) / ((200.0f * 6.0f) / 154000.0f);
    ip.gyroscope.roll = (unpack_float(current_buttons[VANILLA_SENSOR_GYRO_ROLL]) * (180.0f/M_PI)) / ((200.0f * 6.0f) / 154000.0f);

    pthread_mutex_unlock(&button_mtx);

    ip.seq_id = htons(seq_id);
    seq_id++;

    ip.fw_version_neg = 215;

    send_to_console(socket_hid, &ip, sizeof(ip), PORT_HID);
}

void *listen_input(void *x)
{
    gamepad_context_t *info = (gamepad_context_t *) x;

    pthread_mutex_init(&button_mtx, NULL);

    do {
        send_input(info->socket_hid);
        usleep(5 * 1000); // Produces 200Hz input, probably no need to go higher for the Wii U (supposedly the real gamepad is 180Hz)
    } while (!is_interrupted());

    pthread_mutex_destroy(&button_mtx);

    pthread_exit(NULL);
    
    return NULL;
}