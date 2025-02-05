#include "ui.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "ui_anim.h"
#include "ui_priv.h"

vui_context_t *vui_alloc(int width, int height)
{
    vui_context_t *vui = malloc(sizeof(vui_context_t));
    vui_reset(vui);
    vui->platform_data = 0;
    vui->screen_width = width;
    vui->screen_height = height;
    return vui;
}

void vui_free(vui_context_t *vui)
{
    vui_reset(vui);
    free(vui);
}

int vui_reset(vui_context_t *ctx)
{
    ctx->button_count = 0;
    ctx->label_count = 0;
    ctx->rect_count = 0;
    ctx->animation_enabled = 0;
    ctx->button_active = -1;
    ctx->layers = 1;
    ctx->layer_opacity[0] = 1;
}

void vui_get_screen_size(vui_context_t *ctx, int *width, int *height)
{
    if (width) *width = ctx->screen_width;
    if (height) *height = ctx->screen_height;
}

int vui_button_create(vui_context_t *ctx, int x, int y, int w, int h, const char *text, vui_button_style style, int layer, vui_button_callback_t callback, void *callback_data)
{
    if (ctx->button_count == MAX_BUTTON_COUNT) {
        return -1;
    }

    int index = ctx->button_count;

    vui_button_t *btn = &ctx->buttons[index];

    btn->layer = layer;

    // Inset rect by dividing by 1.1
    btn->x = x;
    btn->y = y;
    btn->w = w;
    btn->h = h;

    int new_w = w * 10 / 11;
    int new_h = h * 10 / 11;
    int new_x = x + w / 2 - new_w / 2;
    int new_y = y + h / 2 - new_h / 2;

    vui_button_update_geometry(ctx, index, new_x, new_y, new_w, new_h);
    vui_button_update_text(ctx, index, text);
    vui_button_update_style(ctx, index, style);
    vui_button_update_click_handler(ctx, index, callback, callback_data);

    ctx->button_count++;

    return index;
}

void vui_button_update_click_handler(vui_context_t *ctx, int index, vui_button_callback_t handler, void *userdata)
{
    vui_button_t *btn = &ctx->buttons[index];
    btn->onclick = handler;
    btn->onclick_data = userdata;
}

void vui_button_get_geometry(vui_context_t *ctx, int index, int *x, int *y, int *w, int *h)
{
    vui_button_t *btn = &ctx->buttons[index];
    *x = btn->sx;
    *y = btn->sy;
    *w = btn->sw;
    *h = btn->sh;
}

void vui_button_update_geometry(vui_context_t *ctx, int index, int x, int y, int w, int h)
{
    vui_button_t *btn = &ctx->buttons[index];
    btn->sx = x;
    btn->sy = y;
    btn->sw = w;
    btn->sh = h;
}

void vui_strcpy(char *dst, const char *src)
{
    if (src)
        strncpy(dst, src, MAX_BUTTON_TEXT);
    else
        dst[0] = 0;
    dst[MAX_BUTTON_TEXT-1] = 0;
}

void vui_button_update_text(vui_context_t *ctx, int index, const char *text)
{
    // Copy text (enforce null terminator)
    vui_button_t *btn = &ctx->buttons[index];
    vui_strcpy(btn->text, text);
}

void vui_button_update_style(vui_context_t *ctx, int index, vui_button_style style)
{
    vui_button_t *btn = &ctx->buttons[index];
    btn->style = style;
}

int point_inside_button(vui_button_t *btn, int x, int y)
{
    return x >= btn->x && y >= btn->y && x < btn->x + btn->w && y < btn->y + btn->h;
}

void vui_process_mousedown(vui_context_t *ctx, int x, int y)
{
    if (ctx->button_active != -1) {
        // Button already mousedown'd, ignore this
        return;
    }

    if (ctx->animation_enabled) {
        return;
    }

    for (int i = 0; i < ctx->button_count; i++) {
        vui_button_t *btn = &ctx->buttons[i];

        if (point_inside_button(btn, x, y)) {
            vui_animation_button_mousedown(ctx, i, 0, 0);
            ctx->button_active = i;
            break;
        }
    }
}

void vui_button_callback(vui_context_t *ctx, void *data)
{
    int index = (int) (intptr_t) data;
    vui_button_t *btn = &ctx->buttons[index];
    if (btn->onclick) {
        btn->onclick(ctx, ctx->button_active, btn->onclick_data);
    }
}

void vui_process_mouseup(vui_context_t *ctx, int x, int y)
{
    if (ctx->button_active == -1) {
        // No button was mousedown'd, do nothing here
        return;
    }

    vui_button_t *btn = &ctx->buttons[ctx->button_active];

    // Only do action if mouse is still inside button's rect
    vui_callback_t callback = 0;
    void *callback_data = 0;
    if (point_inside_button(btn, x, y)) {
        callback = vui_button_callback;
        callback_data = (void *) (intptr_t) ctx->button_active;
    }

    vui_animation_button_mouseup(ctx, ctx->button_active, callback, callback_data);

    ctx->button_active = -1;
}

void vui_cancel_animation(vui_context_t *ctx)
{
    if (ctx->animation_enabled) {
        // Finish current animation immediately before starting next one
        if (ctx->animation.step) {
            ctx->animation.step(ctx, ctx->animation.length, ctx->animation.step_data);
        }
        if (ctx->animation.complete) {
            ctx->animation.complete(ctx, ctx->animation.complete_data);
        }
    }
}

void vui_start_animation(vui_context_t *ctx, int64_t length, vui_anim_step_callback_t step, void *step_data, vui_callback_t complete, void *complete_data)
{
    vui_cancel_animation(ctx);

    ctx->animation_enabled = 1;
    ctx->animation.progress = 0;
    ctx->animation.length = length;
    ctx->animation.step = step;
    ctx->animation.step_data = step_data;
    ctx->animation.complete = complete;
    ctx->animation.complete_data = complete_data;
    gettimeofday(&ctx->animation.start_time, NULL);

    ctx->animation.step(ctx, 0, ctx->animation.step_data);
}

void vui_update(vui_context_t *ctx)
{
    if (ctx->animation_enabled) {
        struct timeval now;
        gettimeofday(&now, NULL);

        int64_t diff = (now.tv_sec - ctx->animation.start_time.tv_sec) * 1000000 + (now.tv_usec - ctx->animation.start_time.tv_usec);
        
        if (diff > ctx->animation.length) {
            diff = ctx->animation.length;
        }
        
        ctx->animation.step(ctx, diff, ctx->animation.step_data);

        if (diff == ctx->animation.length) {
            ctx->animation.step(ctx, ctx->animation.length, ctx->animation.step_data);
            ctx->animation_enabled = 0;
            if (ctx->animation.complete) {
                ctx->animation.complete(ctx, ctx->animation.complete_data);
            }
        }
    }
}

int vui_layer_create(vui_context_t *ctx)
{
    int cur_layer = ctx->layers;
    if (cur_layer == MAX_BUTTON_COUNT) {
        return -1;
    }

    ctx->layers++;
    
    // Set layer defaults
    ctx->layer_opacity[cur_layer] = 1.0f;

    vui_color_t *layerbg = &ctx->layer_color[cur_layer];
    layerbg->r = 0;
    layerbg->g = 0;
    layerbg->b = 0;
    layerbg->a = 0;
    
    return cur_layer;
}

int vui_layer_destroy(vui_context_t *ctx)
{
    int cur_layer = ctx->layers;
    if (cur_layer == 1) {
        return -1;
    }

    ctx->layers--;

    return cur_layer;
}

void vui_layer_set_opacity(vui_context_t *ctx, int layer, float opacity)
{
    ctx->layer_opacity[layer] = opacity;
}

vui_color_t vui_color_create(float r, float g, float b, float a)
{
    vui_color_t c;
    c.r = r;
    c.g = g;
    c.b = b;
    c.a = a;
    return c;
}

void vui_layer_set_bgcolor(vui_context_t *ctx, int layer, vui_color_t color)
{
    ctx->layer_color[layer] = color;
}

int vui_label_create(vui_context_t *ctx, int x, int y, int w, int h, const char *text, vui_color_t color, int layer)
{
    if (ctx->label_count == MAX_BUTTON_COUNT) {
        return -1;
    }

    int index = ctx->label_count;

    vui_label_t *lbl = &ctx->labels[index];

    lbl->x = x;
    lbl->y = y;
    lbl->w = w;
    lbl->h = h;

    lbl->color = color;

    lbl->layer = layer;
    vui_strcpy(lbl->text, text);

    ctx->label_count++;

    return index;
}

int vui_rect_create(vui_context_t *ctx, int x, int y, int w, int h, int border_radius, vui_color_t color, int layer)
{
    if (ctx->rect_count == MAX_BUTTON_COUNT) {
        return -1;
    }

    int index = ctx->rect_count;

    vui_rect_priv_t *rect = &ctx->rects[index];

    rect->x = x;
    rect->y = y;
    rect->w = w;
    rect->h = h;

    rect->layer = layer;
    rect->color = color;
    rect->border_radius = border_radius;

    ctx->rect_count++;

    return index;
}