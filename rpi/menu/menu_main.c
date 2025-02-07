#include "menu_main.h"

#include <stdio.h>

#include "menu_common.h"
#include "menu_sync.h"
#include "ui/ui_anim.h"

#define MAIN_MENU_ENTRIES 3

static int main_menu_btns[MAIN_MENU_ENTRIES] = {0};

static int main_menu_first_run = 1;
static int main_menu_current = 0;

static int console_menu_layer = 0;

void vanilla_menu_main_sync_action(vui_context_t *vui, int btn, void *v)
{
    vui_transition_fade_layer_out(vui, (int) (intptr_t) v, vanilla_menu_sync, 0);
}

void vanilla_menu_main_populate(vui_context_t *vui)
{
    for (int i = 0; i < MAIN_MENU_ENTRIES; i++) {
        int b = main_menu_btns[i];
        char buf[64];
        snprintf(buf, sizeof(buf), "Wii U %i", main_menu_current + i + 1);
        vui_button_update_text(vui, b, buf);
    }
}

void vanilla_menu_main_finish_transition(vui_context_t *vui, void *v)
{
    intptr_t data = (intptr_t) v;

    int layer = data & 0xFF;
    int target = data >> 8;

    main_menu_current = target;

    vanilla_menu_main_populate(vui);

    vui_transition_fade_layer_in(vui, layer, 0, 0);
}

void vanilla_menu_main_start_transition(vui_context_t *vui, int layer, int to)
{
    if (to < 0) {
        // Can't go into negatives
        return;
    }

    vui_transition_fade_layer_out(vui, layer, vanilla_menu_main_finish_transition, (void *) (intptr_t) (layer | to << 8));
}

void vanilla_menu_main_left_arrow(vui_context_t *vui, int btn, void *v)
{
    vanilla_menu_main_start_transition(vui, (intptr_t) v, main_menu_current - MAIN_MENU_ENTRIES);
}

void vanilla_menu_main_right_arrow(vui_context_t *vui, int btn, void *v)
{
    vanilla_menu_main_start_transition(vui, (intptr_t) v, main_menu_current + MAIN_MENU_ENTRIES);
}

void vanilla_menu_main(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    int SCREEN_WIDTH, SCREEN_HEIGHT;
    vui_get_screen_size(vui, &SCREEN_WIDTH, &SCREEN_HEIGHT);

    int arrow_y = SCREEN_HEIGHT / 2 - BTN_SZ / 2;

    // Layer
    int layer = vui_layer_create(vui);
    console_menu_layer = vui_layer_create(vui);

    // Left arrow
    vui_button_create(vui, 0, arrow_y, BTN_SZ, BTN_SZ, "<", NULL, VUI_BUTTON_STYLE_BUTTON, layer, vanilla_menu_main_left_arrow, (void *) (intptr_t) console_menu_layer);

    // Right arrow
    vui_button_create(vui, SCREEN_WIDTH-BTN_SZ, arrow_y, BTN_SZ, BTN_SZ, ">", NULL, VUI_BUTTON_STYLE_BUTTON, layer, vanilla_menu_main_right_arrow, (void *) (intptr_t) console_menu_layer);

    // Sync button
    vui_button_create(vui, BTN_SZ, SCREEN_HEIGHT-BTN_SZ, (SCREEN_WIDTH-BTN_SZ*2)/2, BTN_SZ, "Sync", NULL, VUI_BUTTON_STYLE_BUTTON, layer, vanilla_menu_main_sync_action, (void *) (intptr_t) layer);

    // Connect button
    vui_button_create(vui, BTN_SZ + (SCREEN_WIDTH-BTN_SZ*2)/2, SCREEN_HEIGHT-BTN_SZ, (SCREEN_WIDTH-BTN_SZ*2)/2, BTN_SZ, "Connect", "/home/matt/src/vanilla/rpi/tex/red.png", VUI_BUTTON_STYLE_BUTTON, layer, NULL, NULL);

    // Delete button
    vui_button_create(vui, 0, 0, BTN_SZ, BTN_SZ, "Edit", NULL, VUI_BUTTON_STYLE_CORNER, layer, NULL, NULL);
    vui_button_create(vui, SCREEN_WIDTH-BTN_SZ, 0, BTN_SZ, BTN_SZ, "Settings", NULL, VUI_BUTTON_STYLE_CORNER, layer, NULL, NULL);

    // Console list
    int list_item_width = SCREEN_WIDTH - BTN_SZ - BTN_SZ;
    int list_item_height = (SCREEN_HEIGHT - BTN_SZ - BTN_SZ) / MAIN_MENU_ENTRIES;
    for (int i = 0; i < MAIN_MENU_ENTRIES; i++) {
        main_menu_btns[i] = vui_button_create(vui, BTN_SZ, BTN_SZ + list_item_height * i, list_item_width, list_item_height, NULL, NULL, VUI_BUTTON_STYLE_LIST, console_menu_layer, NULL, NULL);
    }
    vanilla_menu_main_populate(vui);

    if (main_menu_first_run) {
        vui_transition_fade_black_out(vui, NULL, NULL);
        main_menu_first_run = 0;
    } else {
        vui_transition_fade_layer_in(vui, layer, NULL, NULL);
    }
}