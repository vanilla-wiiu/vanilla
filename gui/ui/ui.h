#ifndef VANILLA_PI_UI_H
#define VANILLA_PI_UI_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    VUI_BUTTON_STYLE_NONE,
    VUI_BUTTON_STYLE_BUTTON,
    VUI_BUTTON_STYLE_CORNER,
    VUI_BUTTON_STYLE_LIST
} vui_button_style_t;

typedef enum {
    VUI_FONT_SIZE_NORMAL,
    VUI_FONT_SIZE_SMALL,
    VUI_FONT_SIZE_TINY,
} vui_font_size_t;

typedef enum {
    VUI_POWERSTATE_ERROR = -1,
    VUI_POWERSTATE_UNKNOWN,
    VUI_POWERSTATE_ON_BATTERY,
    VUI_POWERSTATE_NO_BATTERY,
    VUI_POWERSTATE_CHARGING,
    VUI_POWERSTATE_CHARGED
} vui_power_state_t;

typedef struct {
    float r;
    float g;
    float b;
    float a;
} vui_color_t;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} vui_rect_t;

typedef struct vui_context_t vui_context_t;

typedef void (*vui_callback_t)(vui_context_t *ctx, void *userdata);
typedef void (*vui_button_callback_t)(vui_context_t *ctx, int button, void *userdata);
typedef void (*vui_anim_step_callback_t)(vui_context_t *ctx, int64_t time, void *userdata);
typedef void (*vui_mic_callback_t)(void *userdata, const uint8_t *stream, size_t len);

typedef enum {
    VUI_DIR_LEFT,
    VUI_DIR_RIGHT,
    VUI_DIR_UP,
    VUI_DIR_DOWN,
} vui_direction_t;

/**
 * Context-related functions
 */
vui_context_t *vui_alloc(int width, int height);
void vui_reset(vui_context_t *ctx);
void vui_free(vui_context_t *ctx);
void vui_update(vui_context_t *ctx);
int vui_game_mode_get(vui_context_t *ctx);
void vui_game_mode_set(vui_context_t *ctx, int enabled);
int vui_get_font_height(vui_context_t *ctx, vui_font_size_t size);
void vui_quit(vui_context_t *ctx);

/**
 * Audio-related functions
 */
void vui_audio_push(vui_context_t *ctx, const void *data, size_t size);
void vui_audio_set_enabled(vui_context_t *ctx, int enabled);

/**
 * Screen-related functions
 */
void vui_get_screen_size(vui_context_t *ctx, int *width, int *height);
void vui_enable_background(vui_context_t *ctx, int enabled);
void vui_set_background(vui_context_t *ctx, const char *background_image);

/**
 * Layer-related functions
 */
int vui_layer_create(vui_context_t *ctx);
void vui_layer_set_opacity(vui_context_t *ctx, int layer, float opacity);
void vui_layer_set_enabled(vui_context_t *ctx, int layer, int enabled);
void vui_layer_set_bgcolor(vui_context_t *ctx, int layer, vui_color_t color);
int vui_layer_destroy(vui_context_t *ctx);

/**
 * Button-related functions
 */
int vui_button_create(vui_context_t *ctx, int x, int y, int w, int h, const char *text, const char *icon, vui_button_style_t style, int layer, vui_button_callback_t callback, void *callback_data);
void vui_button_get_geometry(vui_context_t *ctx, int button, int *x, int *y, int *w, int *h);
int vui_button_get_checked(vui_context_t *ctx, int button);
void vui_button_update_click_handler(vui_context_t *ctx, int index, vui_button_callback_t handler, void *userdata);
void vui_button_update_geometry(vui_context_t *ctx, int button, int x, int y, int w, int h);
void vui_button_update_icon(vui_context_t *ctx, int button, const char *icon);
void vui_button_update_text(vui_context_t *ctx, int button, const char *text);
void vui_button_update_style(vui_context_t *ctx, int button, vui_button_style_t style);
void vui_button_update_visible(vui_context_t *ctx, int button, int visible);
void vui_button_update_enabled(vui_context_t *ctx, int button, int enabled);
void vui_button_update_checked(vui_context_t *ctx, int button, int checked);
void vui_button_update_checkable(vui_context_t *ctx, int button, int checkable);
void vui_button_set_cancel(vui_context_t *ctx, int button);

/**
 * Label-related functions
 */
int vui_label_create(vui_context_t *ctx, int x, int y, int w, int h, const char *text, vui_color_t color, vui_font_size_t size, int layer);
void vui_label_update_text(vui_context_t *ctx, int label, const char *text);
void vui_label_update_visible(vui_context_t *ctx, int label, int visible);

/**
 * TextEdit-related functions
 */
int vui_textedit_create(vui_context_t *ctx, int x, int y, int w, int h, const char *initial_text, vui_font_size_t size, int password, int layer);
void vui_textedit_get_text(vui_context_t *ctx, int textedit, char *output, size_t output_size);
void vui_textedit_update_text(vui_context_t *ctx, int textedit, const char *text);
void vui_textedit_update_visible(vui_context_t *ctx, int textedit, int visible);
void vui_textedit_update_enabled(vui_context_t *ctx, int textedit, int enabled);
void vui_textedit_input(vui_context_t *ctx, int textedit, const char *text);
void vui_textedit_backspace(vui_context_t *ctx, int textedit);
void vui_textedit_del(vui_context_t *ctx, int textedit);
void vui_textedit_move_cursor(vui_context_t *ctx, int textedit, int movement);
void vui_textedit_set_cursor(vui_context_t *ctx, int textedit, int pos);

/**
 * Rect-related functions
 */
int vui_rect_create(vui_context_t *ctx, int x, int y, int w, int h, int border_radius, vui_color_t color, int layer);

/**
 * Image-related functions
 */
int vui_image_create(vui_context_t *ctx, int x, int y, int w, int h, const char *image, int layer);
void vui_image_update(vui_context_t *ctx, int image, const char *file);
void vui_image_destroy(vui_context_t *ctx, int image);

/**
 * Color-related functions
 */
vui_color_t vui_color_create(float r, float g, float b, float a);

/**
 * Animation-related functions
 */
void vui_start_animation(vui_context_t *ctx, int64_t length, vui_anim_step_callback_t step, void *step_data, vui_callback_t complete, void *complete_data);
void vui_cancel_animation(vui_context_t *ctx);
int vui_start_passive_animation(vui_context_t *ctx, vui_anim_step_callback_t step, void *step_data);

/**
 * Input-related functions
 */
void vui_process_mousedown(vui_context_t *ctx, int x, int y);
void vui_process_mouseup(vui_context_t *ctx, int x, int y);
void vui_process_keydown(vui_context_t *ctx, int button);
void vui_process_keyup(vui_context_t *ctx, int button);
void vui_vibrate_set(vui_context_t *ctx, uint8_t val);

/**
 * Power-related functions
 */
vui_power_state_t vui_power_state_get(vui_context_t *ctx, int *percent);

/**
 * Microphone-related functions
 */
void vui_mic_enabled_set(vui_context_t *ctx, int enabled);
void vui_mic_callback_set(vui_context_t *ctx, vui_mic_callback_t callback, void *userdata);

#endif // VANILLA_PI_UI_H
