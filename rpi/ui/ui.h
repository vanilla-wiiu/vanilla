#ifndef VANILLA_PI_UI_H
#define VANILLA_PI_UI_H

#include <stdint.h>

typedef enum {
    VUI_BUTTON_STYLE_NONE,
    VUI_BUTTON_STYLE_BUTTON,
    VUI_BUTTON_STYLE_CORNER,
    VUI_BUTTON_STYLE_LIST
} vui_button_style;

typedef struct vui_context_t vui_context_t;

typedef void (*vui_callback_t)(vui_context_t *ctx, void *userdata);
typedef void (*vui_button_callback_t)(vui_context_t *ctx, int button, void *userdata);
typedef void (*vui_anim_step_callback_t)(vui_context_t *ctx, int64_t time, void *userdata);

/**
 * Context-related functions
 */
vui_context_t *vui_alloc();
int vui_reset(vui_context_t *ctx);
void vui_free(vui_context_t *ctx);
void vui_update(vui_context_t *ctx);

/**
 * Button-related functions
 */
int vui_button_create(vui_context_t *ctx, int x, int y, int w, int h, const char *text, vui_button_style style, vui_button_callback_t onclick, void *onclick_data);
void vui_button_get_geometry(vui_context_t *ctx, int button, int *x, int *y, int *w, int *h);
void vui_button_update_geometry(vui_context_t *ctx, int button, int x, int y, int w, int h);
void vui_button_update_text(vui_context_t *ctx, int button, const char *text);
void vui_button_update_onclick(vui_context_t *ctx, int button, vui_button_callback_t onclick, void *onclick_data);
void vui_button_update_style(vui_context_t *ctx, int button, vui_button_style style);

/**
 * Animation-related functions
 */
void vui_start_animation(vui_context_t *ctx, int64_t length, vui_anim_step_callback_t step, void *step_data, vui_callback_t complete, void *complete_data);

/**
 * Input-related functions
 */
void vui_process_click(vui_context_t *ctx, int x, int y);

/**
 * Miscellaneous functions
 */
void vui_set_overlay_color(vui_context_t *vui, int enabled, float r, float g, float b, float a);

#endif // VANILLA_PI_UI_H