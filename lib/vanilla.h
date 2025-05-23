#ifndef VANILLA_SERVER_H
#define VANILLA_SERVER_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#define VANILLA_SUCCESS                  0
#define VANILLA_ERR_GENERIC             -1
#define VANILLA_ERR_UNKNOWN_COMMAND     -2
#define VANILLA_ERR_INVALID_ARGUMENT    -3
#define VANILLA_ERR_PIPE_UNRESPONSIVE   -4
#define VANILLA_ERR_OUT_OF_MEMORY       -5
#define VANILLA_ERR_BUSY                -6
#define VANILLA_ERR_BAD_SOCKET          -7
#define VANILLA_ERR_NO_CONNECTION       -8
#define VANILLA_ERR_SHUTDOWN            -9
#define VANILLA_ERR_CONNECTED           -10
#define VANILLA_ERR_DISCONNECTED        -11

static const uint32_t VANILLA_ADDRESS_LOCAL = 0xFFFFFFFF;
static const uint32_t VANILLA_ADDRESS_DIRECT = 0x0;

enum VanillaGamepadButtons
{
    VANILLA_BTN_A,
    VANILLA_BTN_B,
    VANILLA_BTN_X,
    VANILLA_BTN_Y,
    VANILLA_BTN_L,
    VANILLA_BTN_R,
    VANILLA_BTN_ZL,
    VANILLA_BTN_ZR,
    VANILLA_BTN_MINUS,
    VANILLA_BTN_PLUS,
    VANILLA_BTN_HOME,
    VANILLA_BTN_TV,
    VANILLA_BTN_L3,
    VANILLA_BTN_R3,
    VANILLA_BTN_LEFT,
    VANILLA_BTN_RIGHT,
    VANILLA_BTN_DOWN,
    VANILLA_BTN_UP,
    VANILLA_AXIS_L_X,
    VANILLA_AXIS_L_Y,
    VANILLA_AXIS_R_X,
    VANILLA_AXIS_R_Y,
    VANILLA_AXIS_L_LEFT,
    VANILLA_AXIS_L_UP,
    VANILLA_AXIS_L_RIGHT,
    VANILLA_AXIS_L_DOWN,
    VANILLA_AXIS_R_LEFT,
    VANILLA_AXIS_R_UP,
    VANILLA_AXIS_R_RIGHT,
    VANILLA_AXIS_R_DOWN,
    VANILLA_AXIS_VOLUME,
    VANILLA_SENSOR_ACCEL_X,
    VANILLA_SENSOR_ACCEL_Y,
    VANILLA_SENSOR_ACCEL_Z,
    VANILLA_SENSOR_GYRO_PITCH,
    VANILLA_SENSOR_GYRO_YAW,
    VANILLA_SENSOR_GYRO_ROLL,
    VANILLA_BTN_COUNT
};

enum VanillaEvent
{
    VANILLA_EVENT_NONE,
    VANILLA_EVENT_VIDEO,
    VANILLA_EVENT_AUDIO,
    VANILLA_EVENT_VIBRATE,
    VANILLA_EVENT_SYNC,
    VANILLA_EVENT_ERROR,
	VANILLA_EVENT_MIC
};

enum VanillaRegion
{
    VANILLA_REGION_JAPAN         = 0,
    VANILLA_REGION_AMERICA       = 1,
    VANILLA_REGION_EUROPE        = 2,
    VANILLA_REGION_CHINA         = 3,
    VANILLA_REGION_SOUTH_KOREA   = 4,
    VANILLA_REGION_TAIWAN        = 5,
    VANILLA_REGION_AUSTRALIA     = 6,
};

enum VanillaBatteryStatus {
    VANILLA_BATTERY_STATUS_CHARGING = 0,
    VANILLA_BATTERY_STATUS_UNKNOWN  = 1,
    VANILLA_BATTERY_STATUS_VERY_LOW = 2,
    VANILLA_BATTERY_STATUS_LOW      = 3,
    VANILLA_BATTERY_STATUS_MEDIUM   = 4,
    VANILLA_BATTERY_STATUS_HIGH     = 5,
    VANILLA_BATTERY_STATUS_FULL     = 6
};

typedef struct
{
    int type;
    uint8_t *data;
    size_t size;
} vanilla_event_t;

#pragma pack(push, 1)
typedef struct { unsigned char bssid[6]; } vanilla_bssid_t;
typedef struct { unsigned char psk[32]; } vanilla_psk_t;
typedef struct {
    vanilla_bssid_t bssid;
    vanilla_psk_t psk;
} vanilla_connection_t;
typedef struct {
    int status;
    vanilla_connection_t data;
} vanilla_sync_event_t;
#pragma pack(pop)

/**
 * Start listening for gamepad commands
 */
int vanilla_start(uint32_t server_address, vanilla_bssid_t bssid, vanilla_psk_t psk);
int vanilla_sync(uint16_t code, uint32_t server_address);

int vanilla_poll_event(vanilla_event_t *event);
int vanilla_wait_event(vanilla_event_t *event);
int vanilla_free_event(vanilla_event_t *event);

/**
 * Attempt to stop the current action
 *
 * This can be called from another thread to safely exit a blocking call to vanilla_start().
 */
void vanilla_stop();

/**
 * Set button/axis state
 *
 * This can be called from another thread to change the button state while vanilla_connect_to_console() is running.
 *
 * For buttons, anything non-zero will be considered a press.
 * For axes, the range is signed 16-bit (-32,768 to 32,767).
 * For accelerometers, cast a float value in m/s^2.
 * For gyroscopes, cast a float value in radians per second.
 */
void vanilla_set_button(int button, int32_t value);

/**
 * Set touch screen coordinates to `x` and `y`
 *
 * This can be called from another thread to change the button state while vanilla_connect_to_console() is running.
 *
 * `x` and `y` are expected to be in gamepad screen coordinates (0x0 to 853x479 inclusive).
 * If either `x` or `y` are -1, this point will be disabled.
 */
void vanilla_set_touch(int x, int y);

/**
 * Logging function
 */
void vanilla_log(const char *format, ...);
void vanilla_log_no_newline(const char *format, ...);
void vanilla_log_no_newline_va(const char *format, va_list args);

/**
 * Install custom logger
 */
void vanilla_install_logger(void (*logger)(const char *, va_list args));

/**
 * Request an IDR (instant decoder refresh) video frame from the console
 */
void vanilla_request_idr();

/**
 * Sets the region Vanilla should present itself to the console
 *
 * The gamepad is region locked and the console will complain if the gamepad doesn't match.
 *
 * Set to a member of the VanillaRegion enum.
 */
void vanilla_set_region(int region);

/**
 * Sets the gamepad battery status that Vanilla should send to the console
 */
void vanilla_set_battery_status(int battery_status);

/**
 * Retrieve SPS/PPS parameters for H.264 packeting
 */
size_t vanilla_generate_h264_header(void *data, size_t size);

/**
 * Send microphone audio
 */
void vanilla_send_audio(const void *data, size_t size);

#if defined(__cplusplus)
}
#endif

#endif // VANILLA_SERVER_H
