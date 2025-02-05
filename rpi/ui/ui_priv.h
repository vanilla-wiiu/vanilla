#ifndef VANILLA_PI_UI_PRIV_H
#define VANILLA_PI_UI_PRIV_H

#define MAX_BUTTON_COUNT 16
#define MAX_BUTTON_TEXT 256

typedef struct {
    int x;
    int y;
    int w;
    int h;
    int sx;
    int sy;
    int sw;
    int sh;
    char text[MAX_BUTTON_TEXT];
    vui_button_style style;
    vui_button_callback_t onclick;
    void *onclick_data;
    int layer;
} vui_button_t;

typedef struct {
    int x;
    int y;
    int w;
    int h;
    char text[MAX_BUTTON_TEXT];
    int layer;
    vui_color_t color;
} vui_label_t;

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
    /// @brief System/game time that the animation starts
    struct timeval start_time;

    /// @brief Progress stored in microseconds
    int64_t progress;

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

typedef struct vui_context_t {
    void *platform_data;
    vui_button_t buttons[MAX_BUTTON_COUNT];
    int button_count;
    vui_label_t labels[MAX_BUTTON_COUNT];
    int label_count;
    vui_rect_priv_t rects[MAX_BUTTON_COUNT];
    int rect_count;
    vui_animation_t animation;
    int animation_enabled;
    int button_active;
    int screen_width;
    int screen_height;
    int layers;
    float layer_opacity[MAX_BUTTON_COUNT];
    vui_color_t layer_color[MAX_BUTTON_COUNT];
} vui_context_t;

#endif // VANILLA_PI_UI_PRIV_H