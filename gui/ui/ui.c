#include "ui.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <vanilla.h>

#include "platform.h"
#include "ui_anim.h"
#include "ui_priv.h"
#include "ui_util.h"
#include "menu/menu.h"

vui_context_t *vui_alloc(int width, int height)
{
    vui_context_t *vui = malloc(sizeof(vui_context_t));
    vui->platform_data = 0;
    vui->screen_width = width;
    vui->screen_height = height;
    vui->background_image[0] = 0;
    vui->background_enabled = 0;
    vui->audio_handler = 0;
    vui->vibrate_handler = 0;
    vui->font_height_handler = 0;
    vui->text_open_handler = 0;
    vui->bind_mode = 0;
    vui->key_map = 0;
    vui->key_map_sz = 0;
    vui->button_map = 0;
    vui->button_map_sz = 0;
    vui->axis_map = 0;
    vui->axis_map_sz = 0;
    vui->power_state_handler = 0;
	vui->mic_callback = 0;
	vui->mic_enabled_handler = 0;
	vui->audio_enabled_handler = 0;
	vui->fullscreen_enabled_handler = 0;
    vui->quit = 0;
    vui_reset(vui);
    return vui;
}

void vui_free(vui_context_t *vui)
{
    vui_reset(vui);
    free(vui);
}

void vui_reset(vui_context_t *ctx)
{
    ctx->button_count = 0;
    ctx->label_count = 0;
    ctx->rect_count = 0;
    ctx->textedit_count = 0;
    ctx->game_mode = 0;
    ctx->passive_animation_count = 0;
    memset(ctx->images, 0, sizeof(ctx->images));
    ctx->animation_enabled = 0;
    ctx->button_active = -1;
    ctx->layers = 1;
    ctx->layer_opacity[0] = 1;
	ctx->layer_enabled[0] = 1;
    ctx->selected_button = -1;
    ctx->cancel_button = -1;
    ctx->active_textedit = -1;
	vui_audio_set_enabled(ctx, 0);
    if (ctx->text_open_handler)
        ctx->text_open_handler(ctx, -1, 0, ctx->text_open_handler_data);
}

void vui_get_screen_size(vui_context_t *ctx, int *width, int *height)
{
    if (width) *width = ctx->screen_width;
    if (height) *height = ctx->screen_height;
}

void vui_enable_background(vui_context_t *ctx, int enabled)
{
    ctx->background_enabled = enabled;
}

void vui_set_background(vui_context_t *ctx, const char *background_image)
{
    vui_strncpy(ctx->background_image, background_image, sizeof(ctx->background_image));
}

void vui_set_fullscreen(vui_context_t *ctx, int enabled)
{
    if (ctx->fullscreen_enabled_handler)
        ctx->fullscreen_enabled_handler(ctx, enabled, ctx->fullscreen_enabled_handler_data);
}

int vui_button_create(vui_context_t *ctx, int x, int y, int w, int h, const char *text, const char *icon, vui_button_style_t style, int layer, vui_button_callback_t callback, void *callback_data)
{
    if (ctx->button_count >= MAX_BUTTON_COUNT) {
        vpilog("Could not create button! Max button count met!\n");
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

    int new_w = w;
    int new_h = h;
    int new_x = x;
    int new_y = y;

    if (style != VUI_BUTTON_STYLE_CORNER) {
        new_w = w * 10 / 11;
        new_h = h * 10 / 11;
        new_x = x + w / 2 - new_w / 2;
        new_y = y + h / 2 - new_h / 2;
    }

    vui_button_update_geometry(ctx, index, new_x, new_y, new_w, new_h);
    vui_button_update_text(ctx, index, text);
    vui_button_update_icon(ctx, index, icon);
    vui_button_update_style(ctx, index, style);
    vui_button_update_click_handler(ctx, index, callback, callback_data);
    vui_button_update_visible(ctx, index, 1);
    vui_button_update_enabled(ctx, index, 1);
    vui_button_update_checked(ctx, index, 0);
    vui_button_update_checkable(ctx, index, 0);

    btn->icon_mod = 0;
    btn->ondraw = NULL;
    btn->ondraw_data = NULL;

    ctx->button_count++;

    return index;
}

void vui_button_update_visible(vui_context_t *ctx, int index, int visible)
{
    vui_button_t *btn = &ctx->buttons[index];
    btn->visible = visible;
    if (!visible && ctx->selected_button == index) {
        ctx->selected_button = -1;
    }
}

void vui_button_update_enabled(vui_context_t *ctx, int index, int enabled)
{
    vui_button_t *btn = &ctx->buttons[index];
    btn->enabled = enabled;
    if (!enabled && ctx->selected_button == index) {
        ctx->selected_button = -1;
    }
}

void vui_button_update_checked(vui_context_t *ctx, int index, int checked)
{
    vui_button_t *btn = &ctx->buttons[index];
    btn->checked = checked;
}

void vui_button_update_checkable(vui_context_t *ctx, int index, int checkable)
{
    vui_button_t *btn = &ctx->buttons[index];
    btn->checkable = checkable;
}

void vui_button_set_cancel(vui_context_t *ctx, int button)
{
    ctx->cancel_button = button;
}

void vui_button_update_click_handler(vui_context_t *ctx, int index, vui_button_callback_t handler, void *userdata)
{
    vui_button_t *btn = &ctx->buttons[index];
    btn->onclick = handler;
    btn->onclick_data = userdata;
}

void vui_button_update_draw_handler(vui_context_t *ctx, int index, vui_button_draw_callback_t handler, void *userdata)
{
    vui_button_t *btn = &ctx->buttons[index];
    btn->ondraw = handler;
    btn->ondraw_data = userdata;
}

void vui_button_get_geometry(vui_context_t *ctx, int index, int *x, int *y, int *w, int *h)
{
    vui_button_t *btn = &ctx->buttons[index];
    *x = btn->sx;
    *y = btn->sy;
    *w = btn->sw;
    *h = btn->sh;
}

int vui_button_get_checked(vui_context_t *ctx, int index)
{
    vui_button_t *btn = &ctx->buttons[index];
    return btn->checked;
}

void vui_button_update_geometry(vui_context_t *ctx, int index, int x, int y, int w, int h)
{
    vui_button_t *btn = &ctx->buttons[index];
    btn->sx = x;
    btn->sy = y;
    btn->sw = w;
    btn->sh = h;
}

void vui_button_update_icon(vui_context_t *ctx, int index, const char *icon)
{
    vui_button_t *btn = &ctx->buttons[index];
    vui_strncpy(btn->icon, icon, sizeof(btn->icon));
}

void vui_button_update_icon_mod(vui_context_t *ctx, int index, uint32_t mod)
{
    ctx->buttons[index].icon_mod = mod;
}

void vui_button_update_text(vui_context_t *ctx, int index, const char *text)
{
    vui_button_t *btn = &ctx->buttons[index];
    vui_strncpy(btn->text, text, sizeof(btn->text));
}

void vui_button_update_style(vui_context_t *ctx, int index, vui_button_style_t style)
{
    vui_button_t *btn = &ctx->buttons[index];
    btn->style = style;
}

void vui_select_direction(vui_context_t *ctx, vui_direction_t dir)
{
    int cx, cy;
    if (ctx->selected_button == -1) {
        int sw, sh;
        vui_get_screen_size(ctx, &sw, &sh);
        switch (dir) {
        case VUI_DIR_LEFT:
            cx = sw+sw;
            cy = sh/2;
            break;
        case VUI_DIR_RIGHT:
            cx = -sw;
            cy = sh/2;
            break;
        case VUI_DIR_UP:
            cx = sw/2;
            cy = sh+sh;
            break;
        case VUI_DIR_DOWN:
            cx = sw/2;
            cy = -sh;
            break;
        }
    } else {
        vui_button_t *sel_btn = &ctx->buttons[ctx->selected_button];
        cx = sel_btn->x + sel_btn->w / 2;
        cy = sel_btn->y + sel_btn->h / 2;
    }

    int new_sel = -1;
    int diff = INT_MAX;

    for (int i = 0; i < ctx->button_count; i++) {
        vui_button_t *b = &ctx->buttons[i];
        if (b->visible && b->enabled) {
            int valid = 0;

            // Determine what "direction" this button is in
            int ccx = b->x + b->w / 2;
            int ccy = b->y + b->h / 2;

            int diffx = ccx - cx;
            int diffy = ccy - cy;

            if (diffx == 0 && diffy == 0) {
                // Don't know how to navigate here
                continue;
            }

            vui_direction_t btn_dir;
            int btn_dist;

            int diffx_abs = abs(diffx);
            int diffy_abs = abs(diffy);

            if (diffx_abs > diffy_abs) {
                btn_dist = diffx_abs;
                if (diffx > 0) {
                    btn_dir = VUI_DIR_RIGHT;
                } else {
                    btn_dir = VUI_DIR_LEFT;
                }
            } else {
                btn_dist = diffy_abs;
                if (diffy > 0) {
                    btn_dir = VUI_DIR_DOWN;
                } else {
                    btn_dir = VUI_DIR_UP;
                }
            }

            if (btn_dir == dir) {
                if (btn_dist < diff) {
                    diff = btn_dist;
                    new_sel = i;
                }
            }
        }
    }

    if (new_sel != -1) {
        ctx->selected_button = new_sel;
    }
}

int point_inside_button(vui_button_t *btn, int x, int y)
{
    return x >= btn->x && y >= btn->y && x < btn->x + btn->w && y < btn->y + btn->h;
}

int point_inside_textedit(vui_textedit_t *btn, int x, int y)
{
    return x >= btn->x && y >= btn->y && x < btn->x + btn->w && y < btn->y + btn->h;
}

void vui_button_callback(vui_context_t *ctx, void *data)
{
    int index = (int) (intptr_t) data;
    vui_button_t *btn = &ctx->buttons[index];
    if (btn->onclick) {
        btn->onclick(ctx, index, btn->onclick_data);
    }
}

void press_button(vui_context_t *ctx, int index)
{
    if (ctx->button_active != -1) {
        // Button already mousedown'd, ignore this
        return;
    }

    if (ctx->animation_enabled) {
        return;
    }

    vui_animation_button_mousedown(ctx, index, 0, 0);
    ctx->button_active = index;
}

void release_button(vui_context_t *ctx, int and_click)
{
    if (ctx->button_active == -1) {
        // No button was mousedown'd, do nothing here
        return;
    }

    vui_button_t *btn = &ctx->buttons[ctx->button_active];

    // Only do action if mouse is still inside button's rect
    vui_callback_t callback = 0;
    void *callback_data = 0;
    if (and_click) {
        callback = vui_button_callback;
        callback_data = (void *) (intptr_t) ctx->button_active;
    }

    vui_animation_button_mouseup(ctx, ctx->button_active, callback, callback_data);

    ctx->button_active = -1;
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

    ctx->selected_button = -1;

    int button_pressed = 0;

    for (int i = 0; i < ctx->button_count; i++) {
        vui_button_t *btn = &ctx->buttons[i];

        if (btn->visible && btn->enabled && point_inside_button(btn, x, y) && ctx->layer_enabled[btn->layer]) {
            press_button(ctx, i);
            button_pressed = 1;
            break;
        }
    }

    int new_active_textedit = -1;
    if (!button_pressed) {
        for (int i = 0; i < ctx->textedit_count; i++) {
            vui_textedit_t *edit = &ctx->textedits[i];

            if (edit->visible && edit->enabled && point_inside_textedit(edit, x, y) && ctx->layer_enabled[edit->layer]) {
                new_active_textedit = i;
                break;
            }
        }
    }

    if (new_active_textedit != ctx->active_textedit) {
        if (ctx->active_textedit == -1) {
            // Signal to start text input
            if (ctx->text_open_handler)
                ctx->text_open_handler(ctx, new_active_textedit, 1, ctx->text_open_handler_data);
            gettimeofday(&ctx->active_textedit_time, 0);
        } else if (new_active_textedit == -1) {
            // Signal to stop text input
            if (ctx->text_open_handler)
                ctx->text_open_handler(ctx, -1, 0, ctx->text_open_handler_data);
        }

        // Update value
        ctx->active_textedit = new_active_textedit;
    }
}

void vui_process_mouseup(vui_context_t *ctx, int x, int y)
{
    if (ctx->button_active == -1) {
        // No button was mousedown'd, do nothing here
        return;
    }

    release_button(ctx, point_inside_button(&ctx->buttons[ctx->button_active], x, y));
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

int vui_start_passive_animation(vui_context_t *ctx, vui_anim_step_callback_t step, void *step_data)
{
    int cur_anim = ctx->passive_animation_count;
    if (cur_anim == MAX_BUTTON_COUNT) {
        return -1;
    }

    ctx->passive_animation_count++;

    // Set layer defaults
    vui_animation_t *a = &ctx->passive_animations[cur_anim];
    a->step = step;
    a->step_data = step_data;
    gettimeofday(&a->start_time, NULL);

    return cur_anim;
}

void vui_start_animation(vui_context_t *ctx, int64_t length, vui_anim_step_callback_t step, void *step_data, vui_callback_t complete, void *complete_data)
{
    vui_cancel_animation(ctx);

    ctx->animation_enabled = 1;
    ctx->animation.length = length;
    ctx->animation.step = step;
    ctx->animation.step_data = step_data;
    ctx->animation.complete = complete;
    ctx->animation.complete_data = complete_data;
    gettimeofday(&ctx->animation.start_time, NULL);

    if (ctx->animation.step)
        ctx->animation.step(ctx, 0, ctx->animation.step_data);
}

void vui_update(vui_context_t *ctx)
{
    struct timeval now;
    gettimeofday(&now, NULL);

    if (ctx->animation_enabled) {
        int64_t diff = (now.tv_sec - ctx->animation.start_time.tv_sec) * 1000000 + (now.tv_usec - ctx->animation.start_time.tv_usec);

        if (diff > ctx->animation.length) {
            diff = ctx->animation.length;
        }

        if (ctx->animation.step)
            ctx->animation.step(ctx, diff, ctx->animation.step_data);

        if (diff == ctx->animation.length) {
            if (ctx->animation.step)
                ctx->animation.step(ctx, ctx->animation.length, ctx->animation.step_data);
            ctx->animation_enabled = 0;
            if (ctx->animation.complete) {
                ctx->animation.complete(ctx, ctx->animation.complete_data);
            }
        }
    }

    for (int i = 0; i < ctx->passive_animation_count; i++) {
        vui_animation_t *a = &ctx->passive_animations[i];
        int64_t diff = (now.tv_sec - a->start_time.tv_sec) * 1000000 + (now.tv_usec - a->start_time.tv_usec);
        if (a->step) {
            a->step(ctx, diff, a->step_data);
        }
    }
}

int vui_game_mode_get(vui_context_t *ctx)
{
    return ctx->game_mode;
}

void vui_game_mode_set(vui_context_t *ctx, int enabled)
{
    ctx->game_mode = enabled;
}

int vui_get_font_height(vui_context_t *ctx, vui_font_size_t size)
{
    if (ctx->font_height_handler)
        return ctx->font_height_handler(size, ctx->font_height_handler_data);
    return 0;
}

void vui_quit(vui_context_t *ctx)
{
    ctx->quit = 1;
}

void vui_audio_push(vui_context_t *ctx, const void *data, size_t size)
{
    if (ctx->audio_handler) {
        ctx->audio_handler(data, size, ctx->audio_handler_data);
    }
}

void vui_audio_set_enabled(vui_context_t *ctx, int enabled)
{
	if (ctx->audio_enabled_handler)
		ctx->audio_enabled_handler(ctx, enabled, ctx->audio_enabled_handler_data);
}

void vui_vibrate_set(vui_context_t *ctx, uint8_t val)
{
    if (ctx->vibrate_handler) {
        ctx->vibrate_handler(val, ctx->vibrate_handler_data);
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
	ctx->layer_enabled[cur_layer] = 1;

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

void vui_layer_set_enabled(vui_context_t *ctx, int layer, int enabled)
{
	ctx->layer_enabled[layer] = enabled;
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

int vui_label_create(vui_context_t *ctx, int x, int y, int w, int h, const char *text, vui_color_t color, vui_font_size_t size, int layer)
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
    lbl->size = size;

    lbl->layer = layer;
    vui_label_update_text(ctx, index, text);
    vui_label_update_visible(ctx, index, 1);

    ctx->label_count++;

    return index;
}

void vui_label_update_text(vui_context_t *ctx, int index, const char *text)
{
    vui_label_t *lbl = &ctx->labels[index];
    vui_strncpy(lbl->text, text, sizeof(lbl->text));
}

void vui_label_update_visible(vui_context_t *ctx, int index, int visible)
{
    vui_label_t *lbl = &ctx->labels[index];
    lbl->visible = visible;
}

int vui_textedit_create(vui_context_t *ctx, int x, int y, int w, int h, const char *initial_text, vui_font_size_t size, int password, int layer)
{
    if (ctx->textedit_count == MAX_BUTTON_COUNT) {
        return -1;
    }

    int index = ctx->textedit_count;

    vui_textedit_t *edit = &ctx->textedits[index];

    edit->x = x;
    edit->y = y;
    edit->w = w;
    edit->h = h;
	edit->password = password;

    edit->layer = layer;
    edit->size = size;

    edit->cursor = 0;

    vui_textedit_update_text(ctx, index, initial_text);
    vui_textedit_update_visible(ctx, index, 1);
    vui_textedit_update_enabled(ctx, index, 1);

    if (initial_text) {
        edit->cursor = vui_utf8_cp_len(initial_text);
    }

    ctx->textedit_count++;

    return index;
}

void vui_textedit_get_text(vui_context_t *ctx, int index, char *output, size_t output_size)
{
    vui_textedit_t *edit = &ctx->textedits[index];
    vui_strncpy(output, edit->text, output_size);
}

void vui_textedit_update_text(vui_context_t *ctx, int index, const char *text)
{
    vui_textedit_t *edit = &ctx->textedits[index];
    vui_strncpy(edit->text, text, sizeof(edit->text));

    if (text) {
        size_t len = vui_utf8_cp_len(text);
        if (edit->cursor > len) {
            edit->cursor = len;
        }
    } else {
        edit->cursor = 0;
    }
}

void vui_textedit_update_visible(vui_context_t *ctx, int index, int visible)
{
    vui_textedit_t *edit = &ctx->textedits[index];
    edit->visible = visible;
}

void vui_textedit_update_enabled(vui_context_t *ctx, int index, int enabled)
{
    vui_textedit_t *edit = &ctx->textedits[index];
    edit->enabled = enabled;
}

void vui_textedit_input(vui_context_t *ctx, int index, const char *text)
{
    vui_textedit_t *edit = &ctx->textedits[index];

    // Ensure we don't overflow our buffer
    size_t our_len = strlen(edit->text);
    size_t new_len = strlen(text);
    if ((our_len + new_len) > (sizeof(edit->text) - 1)) {
        return;
    }

    // Get string up to cursor
    char *end = edit->text;
    for (int i = 0; i < edit->cursor; i++) {
        end = vui_utf8_advance(end);
    }

    // Copy first part
    size_t old_len = (end - edit->text);
    char tmp[sizeof(edit->text)];
    strncpy(tmp, edit->text, old_len);
    tmp[old_len] = 0;

    // Concat new insertion
    strcat(tmp, text);

    // Concat rest of string
    strcat(tmp, end);

    // Copy back to textedit
    strcpy(edit->text, tmp);

    // Increment cursor (even if we added multiple chars, we only incremented one code point)
    vui_textedit_set_cursor(ctx, index, edit->cursor + 1);
}

void vui_textedit_backspace(vui_context_t *ctx, int index)
{
    vui_textedit_t *edit = &ctx->textedits[index];
    if (edit->text[0] == 0) {
        return;
    }

    if (edit->cursor == 0) {
        return;
    }

    vui_textedit_set_cursor(ctx, index, edit->cursor - 1);
    vui_textedit_del(ctx, index);
}

void vui_textedit_del(vui_context_t *ctx, int index)
{
    vui_textedit_t *edit = &ctx->textedits[index];

    char buf[MAX_BUTTON_TEXT];

    // Find true byte location
    char *to = edit->text;
    int target = edit->cursor;
    for (int i = 0; i < target; i++) {
        to = vui_utf8_advance(to);
    }

    if (*to == 0) {
        // String ends here, nothing to do
        return;
    }

    char *from = vui_utf8_advance(to);

    strcpy(buf, from);
    strcpy(to, buf);

    vui_textedit_set_cursor(ctx, index, edit->cursor);
}

void vui_textedit_move_cursor(vui_context_t *ctx, int index, int movement)
{
    vui_textedit_t *edit = &ctx->textedits[index];
    int new_cursor = edit->cursor + movement;
    if (new_cursor < 0)
        new_cursor = 0;
    size_t edit_len = vui_utf8_cp_len(edit->text);
    if (new_cursor > edit_len)
        new_cursor = edit_len;

    vui_textedit_set_cursor(ctx, index, new_cursor);
}

void vui_textedit_set_cursor(vui_context_t *ctx, int index, int pos)
{
    vui_textedit_t *edit = &ctx->textedits[index];
    edit->cursor = pos;

    // Store last time of input
    gettimeofday(&ctx->active_textedit_time, 0);
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

int vui_image_create(vui_context_t *ctx, int x, int y, int w, int h, const char *image, int layer)
{
    // Find next valid image
    int index = -1;
    for (int i = 0; i < MAX_BUTTON_COUNT; i++) {
        if (!ctx->images[i].valid) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        return -1;
    }

    // Fill image with information
    vui_image_t *img = &ctx->images[index];

    img->valid = 1;
    img->x = x;
    img->y = y;
    img->w = w;
    img->h = h;

	vui_image_update(ctx, index, image);

    img->layer = layer;

    return index;
}

void vui_image_update(vui_context_t *ctx, int image, const char *file)
{
    vui_image_t *img = &ctx->images[image];
    vui_strncpy(img->image, file, sizeof(img->image));
}

void vui_image_destroy(vui_context_t *ctx, int image)
{
    ctx->images[image].valid = 0;
}

void vui_process_keydown(vui_context_t *ctx, int button)
{
    // If we are in bind mode we need to consume the next key press for the binding
    if(ctx->bind_mode){
        if (ctx->key_map[button] == VPI_ACTION_DISCONNECT){
            ctx->bind_mode = 0;
            return;
        }
        for(int i = 0; i < ctx->key_map_sz; i++){
            if(ctx->key_map[i] == ctx->bind_mode){
                ctx->key_map[i] = ctx->key_map[button];
                break;
            }
        }
        // Treat button as a scancode NOT a Vanilla button in bind mode
        ctx->key_map[button] = ctx->bind_mode;
        ctx->bind_mode = 0;
        return;
    }
    
    switch (button) {
    case VANILLA_BTN_LEFT:
    case VANILLA_AXIS_L_LEFT:
        vui_select_direction(ctx, VUI_DIR_LEFT);
        break;
    case VANILLA_BTN_RIGHT:
    case VANILLA_AXIS_L_RIGHT:
        vui_select_direction(ctx, VUI_DIR_RIGHT);
        break;
    case VANILLA_BTN_UP:
    case VANILLA_AXIS_L_UP:
        vui_select_direction(ctx, VUI_DIR_UP);
        break;
    case VANILLA_BTN_DOWN:
    case VANILLA_AXIS_L_DOWN:
        vui_select_direction(ctx, VUI_DIR_DOWN);
        break;
    case VANILLA_BTN_A:
    case VANILLA_BTN_PLUS:
        if (ctx->selected_button != -1) {
            press_button(ctx, ctx->selected_button);
        } else if (ctx->button_count == 1) {
            press_button(ctx, 0);
        } else {
            for (int i = 0; i < ctx->button_count; i++) {
                vui_button_t *b = &ctx->buttons[i];
                if (b->enabled && b->visible) {
                    ctx->selected_button = i;
                    break;
                }
            }
        }
        break;
    case VANILLA_BTN_B:
        if (ctx->cancel_button != -1) {
            press_button(ctx, ctx->cancel_button);
        } else if (ctx->button_count == 1) {
            press_button(ctx, 0);
        }
        break;
    }
}

void vui_process_keyup(vui_context_t *ctx, int button)
{
    switch (button) {
    case VANILLA_BTN_A:
    case VANILLA_BTN_B:
        release_button(ctx, 1);
        break;
    }
}

vui_power_state_t vui_power_state_get(vui_context_t *ctx, int *percent)
{
    if (ctx->power_state_handler) {
        return ctx->power_state_handler(ctx, percent);
    } else {
        if (percent) {
            *percent = -1;
        }
        return VUI_POWERSTATE_UNKNOWN;
    }
}

void vui_mic_enabled_set(vui_context_t *ctx, int enabled)
{
	if (ctx->mic_enabled_handler) {
		ctx->mic_enabled_handler(ctx, enabled, ctx->mic_enabled_handler_data);
	}
}

void vui_mic_callback_set(vui_context_t *ctx, vui_mic_callback_t callback, void *userdata)
{
	ctx->mic_callback = callback;
	ctx->mic_callback_data = userdata;
}
