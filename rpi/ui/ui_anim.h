#ifndef VANILLA_PI_UI_ANIM_H
#define VANILLA_PI_UI_ANIM_H

#include <stdint.h>

#include "ui.h"

void vui_transition_fade_layer_in(vui_context_t *ctx, int layer, vui_callback_t callback, void *callback_data);
void vui_transition_fade_layer_out(vui_context_t *ctx, int layer, vui_callback_t callback, void *callback_data);

void vui_transition_fade_black_in(vui_context_t *ctx, vui_callback_t callback, void *callback_data);
void vui_transition_fade_black_out(vui_context_t *ctx, vui_callback_t callback, void *callback_data);

void vui_animation_button_mousedown(vui_context_t *vui, int button, vui_callback_t callback, void *callback_data);
void vui_animation_button_mouseup(vui_context_t *vui, int button, vui_callback_t callback, void *callback_data);

void vui_timer(vui_context_t *vui, uint32_t us, vui_callback_t callback, void *callback_data);

#endif // VANILLA_PI_UI_ANIM_H