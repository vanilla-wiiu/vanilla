#ifndef VANILLA_PI_UI_H
#define VANILLA_PI_UI_H

#include <stdint.h>

typedef enum {
    VUI_BUTTON_STYLE_NONE,
    VUI_BUTTON_STYLE_BUTTON,
    VUI_BUTTON_STYLE_CORNER,
    VUI_BUTTON_STYLE_LIST
} vui_button_style_t;

typedef enum {
    VUI_FONT_SIZE_NORMAL,
    VUI_FONT_SIZE_SMALL
} vui_font_size_t;

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

/**
 * Context-related functions
 */
vui_context_t *vui_alloc(int width, int height);
int vui_reset(vui_context_t *ctx);
void vui_free(vui_context_t *ctx);
void vui_update(vui_context_t *ctx);

/**
 * Screen-related functions
 */
void vui_get_screen_size(vui_context_t *ctx, int *width, int *height);
void vui_set_background(vui_context_t *ctx, const char *background_image);

/**
 * Layer-related functions
 */
int vui_layer_create(vui_context_t *ctx);
void vui_layer_set_opacity(vui_context_t *ctx, int layer, float opacity);
void vui_layer_set_bgcolor(vui_context_t *ctx, int layer, vui_color_t color);
int vui_layer_destroy(vui_context_t *ctx);

/**
 * Button-related functions
 */
int vui_button_create(vui_context_t *ctx, int x, int y, int w, int h, const char *text, const char *icon, vui_button_style_t style, int layer, vui_button_callback_t callback, void *callback_data);
void vui_button_get_geometry(vui_context_t *ctx, int button, int *x, int *y, int *w, int *h);
void vui_button_update_click_handler(vui_context_t *ctx, int index, vui_button_callback_t handler, void *userdata);
void vui_button_update_geometry(vui_context_t *ctx, int button, int x, int y, int w, int h);
void vui_button_update_icon(vui_context_t *ctx, int button, const char *icon);
void vui_button_update_text(vui_context_t *ctx, int button, const char *text);
void vui_button_update_style(vui_context_t *ctx, int button, vui_button_style_t style);

/**
 * Label-related functions
 */
int vui_label_create(vui_context_t *ctx, int x, int y, int w, int h, const char *text, vui_color_t color, vui_font_size_t size, int layer);
int vui_label_update_text(vui_context_t *ctx, int label, const char *text);

/**
 * Rect-related functions
 */
int vui_rect_create(vui_context_t *ctx, int x, int y, int w, int h, int border_radius, vui_color_t color, int layer);

/**
 * Image-related functions
 */
int vui_image_create(vui_context_t *ctx, int x, int y, int w, int h, const char *image, int layer);
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

#endif // VANILLA_PI_UI_H