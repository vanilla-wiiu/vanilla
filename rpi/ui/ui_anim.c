#include "ui.h"

#include <math.h>
#include <stdlib.h>

typedef struct {
    int btn;
    int ox;
    int oy;
    int ow;
    int oh;
} vui_btn_anim_data_t;
static vui_btn_anim_data_t current_button_anim;

typedef struct {
    int pop_layer;
    vui_callback_t callback;
    void *callback_data;
} vui_anim_callback_t;

#define BUTTON_MOUSEDOWN_ANIM_LENGTH    50000
#define BUTTON_MOUSEUP_ANIM_LENGTH      133333
#define FADE_ANIM_LENGTH                250000

void button_mousedown_animation_step(vui_context_t *ctx, int64_t time, void *v)
{
    vui_btn_anim_data_t *a = (vui_btn_anim_data_t *) v;

    // Create a multiplier that goes from 1.0 to 1.1
    float x = time / (float) BUTTON_MOUSEDOWN_ANIM_LENGTH;
    float y = x * 0.1f + 1.0f;

    int new_w = a->ow * y;
    int new_h = a->oh * y;

    vui_button_update_geometry(
        ctx,
        a->btn,
        a->ox + a->ow/2 - new_w/2,
        a->oy + a->oh/2 - new_h/2,
        new_w,
        new_h
    );
}

void button_mouseup_animation_step(vui_context_t *ctx, int64_t time, void *v)
{
    vui_btn_anim_data_t *a = (vui_btn_anim_data_t *) v;
    
    // Create a multiplier that goes from 1.0 to 1.1
    float x = time / (float) BUTTON_MOUSEUP_ANIM_LENGTH;
    float y = powf(2 * x - 1, 2);

    // 1.1 -> 0.98 -> 1.0

    if (time < BUTTON_MOUSEUP_ANIM_LENGTH/2) {
        y = y * 0.12f + 0.98f;
    } else {
        y = y * 0.02f + 0.98f;
    }

    int new_w = a->ow * y;
    int new_h = a->oh * y;

    vui_button_update_geometry(
        ctx,
        a->btn,
        a->ox + a->ow/2 - new_w/2,
        a->oy + a->oh/2 - new_h/2,
        new_w,
        new_h
    );
}

void fade_layer_in_step(vui_context_t *vui, int64_t time, void *v)
{
    int layer = (int) (intptr_t) v;
    vui_layer_set_opacity(vui, layer, time / (float) FADE_ANIM_LENGTH);
}

void fade_layer_out_step(vui_context_t *vui, int64_t time, void *v)
{
    int layer = (int) (intptr_t) v;
    vui_layer_set_opacity(vui, layer, (FADE_ANIM_LENGTH - time) / (float) FADE_ANIM_LENGTH);
}

void vui_transition_fade_layer_complete(vui_context_t *vui, void *v)
{
    vui_anim_callback_t *data = (vui_anim_callback_t *) v;

    // TODO: Some kind of check to ensure this was the correct layer?
    if (data->pop_layer)
        vui_layer_destroy(vui);

    if (data->callback) {
        data->callback(vui, data->callback_data);
    }

    free(data);
}

void vui_animation_button_mousedown(vui_context_t *vui, int button, vui_callback_t callback, void *callback_data)
{
    vui_cancel_animation(vui);

    vui_btn_anim_data_t *a = &current_button_anim;
    a->btn = button;
    vui_button_get_geometry(vui, button, &a->ox, &a->oy, &a->ow, &a->oh);

    vui_start_animation(vui, BUTTON_MOUSEDOWN_ANIM_LENGTH, button_mousedown_animation_step, a, callback, callback_data);
}

void vui_animation_button_mouseup(vui_context_t *vui, int button, vui_callback_t callback, void *callback_data)
{
    vui_btn_anim_data_t *a = &current_button_anim;
    vui_start_animation(vui, BUTTON_MOUSEUP_ANIM_LENGTH, button_mouseup_animation_step, a, callback, callback_data);
}

void vui_transition_fade_layer(vui_context_t *ctx, int layer, vui_anim_step_callback_t step, vui_callback_t callback, void *callback_data, int pop_layer)
{
    vui_anim_callback_t *data = (vui_anim_callback_t *) malloc(sizeof(vui_anim_callback_t));
    data->callback = callback;
    data->callback_data = callback_data;
    data->pop_layer = pop_layer;
    vui_start_animation(ctx, FADE_ANIM_LENGTH, step, (void *) (intptr_t) layer, vui_transition_fade_layer_complete, data);
}

void vui_transition_fade_layer_in(vui_context_t *ctx, int layer, vui_callback_t callback, void *callback_data)
{
    vui_transition_fade_layer(ctx, layer, fade_layer_in_step, callback, callback_data, 0);
}

void vui_transition_fade_layer_out(vui_context_t *ctx, int layer, vui_callback_t callback, void *callback_data)
{
    vui_transition_fade_layer(ctx, layer, fade_layer_out_step, callback, callback_data, 0);
}

void vui_transition_fade_black_in(vui_context_t *ctx, vui_callback_t callback, void *callback_data)
{
    int layer = vui_layer_create(ctx);
    vui_layer_set_bgcolor(ctx, layer, vui_color_create(0, 0, 0, 1));
    vui_transition_fade_layer(ctx, layer, fade_layer_in_step, callback, callback_data, 1);
}

void vui_transition_fade_black_out(vui_context_t *ctx, vui_callback_t callback, void *callback_data)
{
    int layer = vui_layer_create(ctx);
    vui_layer_set_bgcolor(ctx, layer, vui_color_create(0, 0, 0, 1));
    vui_transition_fade_layer(ctx, layer, fade_layer_out_step, callback, callback_data, 1);
}

void vui_timer(vui_context_t *ctx, uint32_t us, vui_callback_t callback, void *callback_data)
{
    vui_start_animation(ctx, us, 0, 0, callback, callback_data);
}