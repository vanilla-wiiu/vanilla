#include "menu.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int btn;
    int ox;
    int oy;
    int ow;
    int oh;
} vanilla_btn_anim_data_t;

static vanilla_btn_anim_data_t button_anim_data;

#define BUTTON_CLICK_ANIM_LENGTH    150000
#define FADE_ANIM_LENGTH            250000

void button_click_animation_step(vui_context_t *ctx, int64_t time, void *v)
{
    vanilla_btn_anim_data_t *a = (vanilla_btn_anim_data_t *) v;

    float y = sin(time / (float) BUTTON_CLICK_ANIM_LENGTH * M_PI);
    
    y *= 0.2f; // Range will be 0.0 - 0.1

    int new_w = a->ow * (1.0f - y);
    int new_h = a->oh * (1.0f - y);

    vui_button_update_geometry(
        ctx,
        a->btn,
        a->ox + a->ow/2 - new_w/2,
        a->oy + a->oh/2 - new_h/2,
        new_w,
        new_h
    );
}

void fade_to_black_step(vui_context_t *vui, int64_t time, void *v)
{
    vui_set_overlay_color(vui, 1, 0, 0, 0, time / (float) FADE_ANIM_LENGTH);
}

void fade_from_black_step(vui_context_t *vui, int64_t time, void *v)
{
    vui_set_overlay_color(vui, 1, 0, 0, 0, (FADE_ANIM_LENGTH - time) / (float) FADE_ANIM_LENGTH);
}

void vanilla_menu_sync(vui_context_t *vui, void *d)
{
    vui_reset(vui);

    vui_button_create(vui, 640, 360, 100, 100, "Back", VUI_BUTTON_STYLE_BUTTON, NULL, NULL);

    vui_start_animation(vui, FADE_ANIM_LENGTH, fade_from_black_step, NULL, NULL, NULL);
}

void vanilla_default_button_action(vui_context_t *vui, int button, void *v)
{
    vanilla_btn_anim_data_t *a = &button_anim_data;

    a->btn = button;
    vui_button_get_geometry(vui, button, &a->ox, &a->oy, &a->ow, &a->oh);

    vui_start_animation(vui, BUTTON_CLICK_ANIM_LENGTH, button_click_animation_step, a, (vui_callback_t) v, NULL);
}

void vanilla_menu_main_sync_action(vui_context_t *vui, void *v)
{
    vui_start_animation(vui, FADE_ANIM_LENGTH, fade_to_black_step, NULL, vanilla_menu_sync, NULL);
}

void vanilla_menu_main(vui_context_t *vui)
{
    vui_reset(vui);

    const int SCREEN_WIDTH = 854;
    const int SCREEN_HEIGHT = 480;
    const int BTN_SZ = 80;

    int arrow_y = SCREEN_HEIGHT / 2 - BTN_SZ / 2;

    // Left arrow
    vui_button_create(vui, 0, arrow_y, BTN_SZ, BTN_SZ, "<", VUI_BUTTON_STYLE_BUTTON, vanilla_default_button_action, NULL);

    // Right arrow
    vui_button_create(vui, SCREEN_WIDTH-BTN_SZ, arrow_y, BTN_SZ, BTN_SZ, ">", VUI_BUTTON_STYLE_BUTTON, vanilla_default_button_action, NULL);

    // Sync button
    vui_button_create(vui, BTN_SZ, SCREEN_HEIGHT-BTN_SZ, (SCREEN_WIDTH-BTN_SZ*2)/2, BTN_SZ, "Sync", VUI_BUTTON_STYLE_BUTTON, vanilla_default_button_action, vanilla_menu_main_sync_action);

    // Connect button
    vui_button_create(vui, BTN_SZ + (SCREEN_WIDTH-BTN_SZ*2)/2, SCREEN_HEIGHT-BTN_SZ, (SCREEN_WIDTH-BTN_SZ*2)/2, BTN_SZ, "Connect", VUI_BUTTON_STYLE_BUTTON, vanilla_default_button_action, NULL);

    // Delete button
    vui_button_create(vui, 0, 0, BTN_SZ, BTN_SZ, "Delete", VUI_BUTTON_STYLE_CORNER, vanilla_default_button_action, NULL);
    vui_button_create(vui, SCREEN_WIDTH-BTN_SZ, 0, BTN_SZ, BTN_SZ, "Settings", VUI_BUTTON_STYLE_CORNER, vanilla_default_button_action, NULL);

    // Console list
    int list_item_width = SCREEN_WIDTH - BTN_SZ - BTN_SZ;
    int list_item_height = (SCREEN_HEIGHT - BTN_SZ - BTN_SZ) / 4;
    for (int i = 0; i < 4; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Wii U %i", i + 1);
        vui_button_create(vui, BTN_SZ, BTN_SZ + list_item_height * i, list_item_width, list_item_height, buf, VUI_BUTTON_STYLE_LIST, NULL, NULL);
    }

    vui_start_animation(vui, FADE_ANIM_LENGTH, fade_from_black_step, NULL, NULL, NULL);
}

void vanilla_menu_init(vui_context_t *vui)
{
    // Start with main menu
    vanilla_menu_main(vui);
}