#ifndef VANILLA_PI_UI_PRIV_H
#define VANILLA_PI_UI_PRIV_H

#define MAX_BUTTON_COUNT 16
#define MAX_BUTTON_TEXT 16

typedef struct {
    int x;
    int y;
    int w;
    int h;
    char text[MAX_BUTTON_TEXT];
    vui_button_style style;
    vui_button_callback_t onclick;
    void *onclick_data;
} vui_button_t;

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
    vui_button_t buttons[MAX_BUTTON_COUNT];
    int button_count;
    vui_animation_t animation;
    int animation_enabled;
    float overlay_r;
    float overlay_g;
    float overlay_b;
    float overlay_a;
    int overlay_enabled;
} vui_context_t;

#endif // VANILLA_PI_UI_PRIV_H