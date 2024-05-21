#ifndef VANILLA_SERVER_H
#define VANILLA_SERVER_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define VANILLA_SUCCESS              0
#define VANILLA_ERROR               -1
#define VANILLA_READY               -2
#define VANILLA_INFO                -3
#define VANILLA_UNKNOWN_COMMAND     -4
#define VANILLA_INVALID_ARGUMENT    -5

enum VanillaGamepadButtons {
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
    VANILLA_BTN_COUNT
};

enum VanillaEvent
{
    VANILLA_EVENT_VIDEO,
    VANILLA_EVENT_AUDIO,
    VANILLA_EVENT_VIBRATE
};

/**
 * Event handler used by caller to receive events
 */
typedef void (*vanilla_event_handler_t)(void *context, int event_type, const char *data, size_t data_size);

/**
 * Attempt to sync with the console
 * 
 * This will block until the task is over or vanilla_stop() is called from another thread.
 */
int vanilla_sync_with_console(const char *wireless_interface, uint16_t code);  

/**
 * Attempt gameplay connection with console
 * 
 * This will block until the task is over or vanilla_stop() is called from another thread.
 */
int vanilla_connect_to_console(const char *wireless_interface, vanilla_event_handler_t event_handler, void *context);

/**
 * Attempt to stop the current action
 * 
 * This can be called from another thread to safely exit a blocking call to vanilla_sync_with_console() or vanilla_connect_to_console().
 */
void vanilla_stop();

/**
 * Set button/axis state
 * 
 * This can be called from another thread to change the button state while vanilla_connect_to_console() is running.
 * 
 * For buttons, anything non-zero will be considered a press.
 * For axes, the range is -32,768 - 32,767.
 */
void vanilla_set_button(int button, int16_t value);

/**
 * Set touch screen coordinates to `x` and `y`
 * 
 * This can be called from another thread to change the button state while vanilla_connect_to_console() is running.
 * 
 * `x` and `y` are expected to be in gamepad screen coordinates (0x0 to 853x479).
 * If either `x` or `y` are -1, this point will be disabled.
 */
void vanilla_set_touch(int x, int y);

#if defined(__cplusplus)
}
#endif

#endif // VANILLA_SERVER_H