#include "ui.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "ui_priv.h"

vui_context_t *vui_alloc()
{
    return malloc(sizeof(vui_context_t));
}

void vui_free(vui_context_t *vui)
{
    vui_reset(vui);
    free(vui);
}

int vui_reset(vui_context_t *ctx)
{
    ctx->button_count = 0;
    ctx->animation_enabled = 0;
    ctx->overlay_enabled = 0;
}

int vui_button_create(vui_context_t *ctx, int x, int y, int w, int h, const char *text, vui_button_style style, vui_button_callback_t onclick, void *onclick_data)
{
    if (ctx->button_count == MAX_BUTTON_COUNT) {
        return -1;
    }

    int index = ctx->button_count;

    vui_button_update_geometry(ctx, index, x, y, w, h);
    vui_button_update_text(ctx, index, text);
    vui_button_update_onclick(ctx, index, onclick, onclick_data);
    vui_button_update_style(ctx, index, style);

    ctx->button_count++;

    return index;
}

void vui_button_get_geometry(vui_context_t *ctx, int index, int *x, int *y, int *w, int *h)
{
    vui_button_t *btn = &ctx->buttons[index];
    *x = btn->x;
    *y = btn->y;
    *w = btn->w;
    *h = btn->h;
}

void vui_button_update_geometry(vui_context_t *ctx, int index, int x, int y, int w, int h)
{
    vui_button_t *btn = &ctx->buttons[index];
    btn->x = x;
    btn->y = y;
    btn->w = w;
    btn->h = h;
}

void vui_button_update_text(vui_context_t *ctx, int index, const char *text)
{
    // Copy text (enforce null terminator)
    vui_button_t *btn = &ctx->buttons[index];
    strncpy(btn->text, text, MAX_BUTTON_TEXT);
    btn->text[MAX_BUTTON_TEXT-1] = 0;
}

void vui_button_update_onclick(vui_context_t *ctx, int index, vui_button_callback_t onclick, void *onclick_data)
{
    vui_button_t *btn = &ctx->buttons[index];
    btn->onclick = onclick;
    btn->onclick_data = onclick_data;
}

void vui_button_update_style(vui_context_t *ctx, int index, vui_button_style style)
{
    vui_button_t *btn = &ctx->buttons[index];
    btn->style = style;
}

void vui_process_click(vui_context_t *ctx, int x, int y)
{
    // Don't process any inputs while an animation is playing
    if (ctx->animation_enabled) {
        return;
    }

    for (int i = 0; i < ctx->button_count; i++) {
        vui_button_t *btn = &ctx->buttons[i];

        if (x >= btn->x && y >= btn->y && x < btn->x + btn->w && y < btn->y + btn->h) {
            if (btn->onclick) {
                btn->onclick(ctx, i, btn->onclick_data);
            }
            break;
        }
    }
}

void vui_start_animation(vui_context_t *ctx, int64_t length, vui_anim_step_callback_t step, void *step_data, vui_callback_t complete, void *complete_data)
{
    // Prevent second animation from starting while another is active
    if (ctx->animation_enabled) {
        return;
    }

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

void vui_set_overlay_color(vui_context_t *vui, int enabled, float r, float g, float b, float a)
{
    vui->overlay_enabled = enabled;
    vui->overlay_r = r;
    vui->overlay_g = g;
    vui->overlay_b = b;
    vui->overlay_a = a;
}