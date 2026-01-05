#ifndef VANILLA_PI_UI_PRIV_H
#define VANILLA_PI_UI_PRIV_H

#include <sys/time.h>

#include "ui.h"

#define MAX_BUTTON_COUNT 16
#define MAX_BUTTON_TEXT 512

typedef struct vui_button_t{
    int x;
    int y;
    int w;
    int h;
    int sx;
    int sy;
    int sw;
    int sh;
    char text[MAX_BUTTON_TEXT];
    char icon[MAX_BUTTON_TEXT];
    vui_button_style_t style;
    vui_button_callback_t onclick;
    vui_button_draw_callback_t ondraw;
    void *onclick_data;
    void *ondraw_data;
    int layer;
    int visible;
    int enabled;
    int checked;
    int checkable;
} vui_button_t;

typedef struct {
    int x;
    int y;
    int w;
    int h;
    char text[MAX_BUTTON_TEXT];
    int layer;
    vui_color_t color;
    vui_font_size_t size;
    int visible;
} vui_label_t;

typedef struct {
    int x;
    int y;
    int w;
    int h;
    char text[MAX_BUTTON_TEXT];
    int layer;
    vui_font_size_t size;
    int visible;
    int enabled;
    int cursor;
	int password;
} vui_textedit_t;

typedef struct {
    int x;
    int y;
    int w;
    int h;
    int border_radius;
    vui_color_t color;
    int layer;
} vui_rect_priv_t;

typedef struct {
    int valid;
    int x;
    int y;
    int w;
    int h;
    char image[MAX_BUTTON_TEXT];
    int layer;
} vui_image_t;

typedef struct {
    /// @brief System/game time that the animation starts
    struct timeval start_time;

    /// @brief Length stored in microseconds
    int64_t length;

    /// @brief Caller-defined function for whenever the animation steps
    vui_anim_step_callback_t step;

    /// @brief Data to be sent to `step` function per step (can be NULL)
    void *step_data;

    /// @brief Caller-defined function for whenever the animation is complete
    vui_callback_t complete;

    /// @brief
    void *complete_data;
} vui_animation_t;

typedef void (*vui_audio_handler_t)(const void *data, size_t size, void *userdata);
typedef void (*vui_vibrate_handler_t)(uint8_t vibrate, void *userdata);
typedef int (*vui_font_height_handler_t)(vui_font_size_t size, void *userdata);
typedef void (*vui_text_open_handler_t)(vui_context_t *ctx, int textedit, int open, void *userdata);
typedef vui_power_state_t (*vui_power_state_handler_t)(vui_context_t *ctx, int *percent);
typedef void (*vui_audio_enabled_handler_t)(vui_context_t *ctx, int enabled, void *userdata);

typedef struct vui_context_t {
    void *platform_data;
    vui_button_t buttons[MAX_BUTTON_COUNT];
    int button_count;
    vui_label_t labels[MAX_BUTTON_COUNT];
    int label_count;
    vui_rect_priv_t rects[MAX_BUTTON_COUNT];
    int rect_count;
    vui_image_t images[MAX_BUTTON_COUNT];
    int textedit_count;
    vui_textedit_t textedits[MAX_BUTTON_COUNT];
    vui_animation_t animation;
    int animation_enabled;
    vui_animation_t passive_animations[MAX_BUTTON_COUNT];
    int passive_animation_count;
    int button_active;
    int screen_width;
    int screen_height;
    int layers;
    float layer_opacity[MAX_BUTTON_COUNT];
    int layer_enabled[MAX_BUTTON_COUNT];
    vui_color_t layer_color[MAX_BUTTON_COUNT];
    char background_image[MAX_BUTTON_TEXT];
    int background_enabled;
    vui_audio_handler_t audio_handler;
    void *audio_handler_data;
    vui_vibrate_handler_t vibrate_handler;
    void *vibrate_handler_data;
    int game_mode;
    int bind_mode;
    int selected_button;
    int cancel_button;
  int *button_map;
  int button_map_sz;
  int *axis_map;
  int axis_map_sz;
  int *key_map;
  int key_map_sz;
    vui_font_height_handler_t font_height_handler;
    void *font_height_handler_data;
    vui_text_open_handler_t text_open_handler;
    void *text_open_handler_data;
    int active_textedit;
    struct timeval active_textedit_time;
    int quit;
    vui_power_state_handler_t power_state_handler;
	vui_mic_callback_t mic_callback;
	void *mic_callback_data;
	vui_audio_enabled_handler_t mic_enabled_handler;
	void *mic_enabled_handler_data;
	vui_audio_enabled_handler_t audio_enabled_handler;
	void *audio_enabled_handler_data;
	vui_bool_callback_t fullscreen_enabled_handler;
	void *fullscreen_enabled_handler_data;
} vui_context_t;

#endif // VANILLA_PI_UI_PRIV_H
