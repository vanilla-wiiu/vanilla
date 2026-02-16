#include "menu_main.h"

#include <stdio.h>

#include "config.h"
#include "lang.h"
#include "menu_common.h"
#include "menu_edit.h"
#include "menu_game.h"
#include "menu_settings.h"
#include "menu_sync.h"
#include "ui/ui_anim.h"

#define MAIN_MENU_ENTRIES 3

static int main_menu_btns[MAIN_MENU_ENTRIES] = {0};

static int main_menu_current = 0;

static int console_menu_layer = 0;

static int main_menu_left_arrow_btn;
static int main_menu_right_arrow_btn;

static int main_menu_no_console_lbl;

static int main_menu_connect_btn;
static int main_menu_edit_btn;

static const char *checkmark = "checkmark.svg";

void vpi_menu_main_sync_action(vui_context_t *vui, int btn, void *v)
{
    vui_transition_fade_layer_out(vui, (int) (intptr_t) v, vpi_menu_sync, 0);
}

void vpi_menu_main_populate(vui_context_t *vui)
{
    for (int i = 0; i < MAIN_MENU_ENTRIES; i++) {
        int console_index = main_menu_current + i;
        int btn = main_menu_btns[i];

        int valid_console = (console_index < vpi_config.connected_console_count);
        vui_button_update_visible(vui, btn, valid_console);

        if (valid_console) {
            vui_button_update_text(vui, btn, vpi_config.connected_console_entries[console_index].name);
        }

        vui_button_update_checked(vui, btn, 0);
    }

    vui_button_update_visible(vui, main_menu_left_arrow_btn, main_menu_current > 0);
    vui_button_update_visible(vui, main_menu_right_arrow_btn, main_menu_current + MAIN_MENU_ENTRIES < vpi_config.connected_console_count);
    vui_label_update_visible(vui, main_menu_no_console_lbl, vpi_config.connected_console_count == 0);
    vui_button_update_enabled(vui, main_menu_connect_btn, 0);
    vui_button_update_enabled(vui, main_menu_edit_btn, 0);
}

void vpi_menu_main_finish_transition(vui_context_t *vui, void *v)
{
    intptr_t data = (intptr_t) v;

    int layer = data & 0xFF;
    int target = data >> 8;

    main_menu_current = target;

    vpi_menu_main_populate(vui);

    vui_transition_fade_layer_in(vui, layer, 0, 0);
}

void vpi_menu_main_start_transition(vui_context_t *vui, int layer, int to)
{
    vui_transition_fade_layer_out(vui, layer, vpi_menu_main_finish_transition, (void *) (intptr_t) (layer | to << 8));
}

void vpi_menu_main_left_arrow(vui_context_t *vui, int btn, void *v)
{
    vpi_menu_main_start_transition(vui, (intptr_t) v, main_menu_current - MAIN_MENU_ENTRIES);
}

void vpi_menu_main_right_arrow(vui_context_t *vui, int btn, void *v)
{
    vpi_menu_main_start_transition(vui, (intptr_t) v, main_menu_current + MAIN_MENU_ENTRIES);
}

void connect_to_console_from_btn(vui_context_t *vui, int console)
{
    vui_transition_fade_black_in(vui, vpi_menu_game, (void *) (intptr_t) console);
}

void vpi_menu_main_connect_console(vui_context_t *vui, int btn, void *v)
{
    if (vui_button_get_checked(vui, btn)) {
        int index = (intptr_t) v;
        int console = main_menu_current + index;
        connect_to_console_from_btn(vui, console);
    } else {
        for (int i = 0; i < MAIN_MENU_ENTRIES; i++) {
            vui_button_update_checked(vui, main_menu_btns[i], 0);
        }
        vui_button_update_checked(vui, btn, 1);
        vui_button_update_enabled(vui, main_menu_connect_btn, 1);
        vui_button_update_enabled(vui, main_menu_edit_btn, 1);
    }
}

void vpi_menu_main_connect_button_action(vui_context_t *vui, int btn, void *v)
{
    for (int i = 0; i < MAIN_MENU_ENTRIES; i++) {
        if (vui_button_get_checked(vui, main_menu_btns[i])) {
            connect_to_console_from_btn(vui, main_menu_current + i);
            break;
        }
    }
}

void vpi_menu_main_edit_button_action(vui_context_t *vui, int btn, void *v)
{
    int layer = (intptr_t) v;

    int console = -1;

    for (int i = 0; i < MAIN_MENU_ENTRIES; i++) {
        if (vui_button_get_checked(vui, main_menu_btns[i])) {
            console = main_menu_current + i;
            break;
        }
    }

    if (console != -1)
        vui_transition_fade_layer_out(vui, layer, vpi_menu_edit, (void *) (intptr_t) console);
}

void vpi_menu_main_settings_button_action(vui_context_t *vui, int btn, void *v)
{
    int layer = (intptr_t) v;
    vui_transition_fade_layer_out(vui, layer, vpi_menu_settings, 0);
}

void vpi_menu_main(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    vui_enable_background(vui, 1);

    int SCREEN_WIDTH, SCREEN_HEIGHT;
    vui_get_screen_size(vui, &SCREEN_WIDTH, &SCREEN_HEIGHT);

    int arrow_y = SCREEN_HEIGHT / 2 - BTN_SZ / 2;

    // Layer
    int layer = vui_layer_create(vui);
    console_menu_layer = vui_layer_create(vui);

    // Console list (generate this first so the first connected console is the default option)
    int list_item_width = SCREEN_WIDTH - BTN_SZ - BTN_SZ;
    int list_item_height = (SCREEN_HEIGHT - BTN_SZ - BTN_SZ) / MAIN_MENU_ENTRIES;
    for (int i = 0; i < MAIN_MENU_ENTRIES; i++) {
        main_menu_btns[i] = vui_button_create(vui, BTN_SZ, BTN_SZ + list_item_height * i, list_item_width, list_item_height, NULL, NULL, VUI_BUTTON_STYLE_LIST, console_menu_layer, vpi_menu_main_connect_console, (void *) (intptr_t) i);
        vui_button_update_checkable(vui, main_menu_btns[i], 1);
    }

    // Left arrow
    main_menu_left_arrow_btn = vui_button_create(vui, 0, arrow_y, BTN_SZ, BTN_SZ, "<", NULL, VUI_BUTTON_STYLE_BUTTON, layer, vpi_menu_main_left_arrow, (void *) (intptr_t) console_menu_layer);

    // Right arrow
    main_menu_right_arrow_btn = vui_button_create(vui, SCREEN_WIDTH-BTN_SZ, arrow_y, BTN_SZ, BTN_SZ, ">", NULL, VUI_BUTTON_STYLE_BUTTON, layer, vpi_menu_main_right_arrow, (void *) (intptr_t) console_menu_layer);

    // Sync button
    vui_button_create(vui, BTN_SZ, SCREEN_HEIGHT-BTN_SZ, (SCREEN_WIDTH-BTN_SZ*2)/2, BTN_SZ, lang(VPI_LANG_SYNC_BTN), NULL, VUI_BUTTON_STYLE_BUTTON, layer, vpi_menu_main_sync_action, (void *) (intptr_t) layer);

    // Connect button
    main_menu_connect_btn = vui_button_create(vui, BTN_SZ + (SCREEN_WIDTH-BTN_SZ*2)/2, SCREEN_HEIGHT-BTN_SZ, (SCREEN_WIDTH-BTN_SZ*2)/2, BTN_SZ, lang(VPI_LANG_CONNECT_BTN), NULL, VUI_BUTTON_STYLE_BUTTON, layer, vpi_menu_main_connect_button_action, NULL);

    // Edit/settings corner button
    int corner_btn_sz = BTN_SZ;
    main_menu_edit_btn = vui_button_create(vui, 0, 0, corner_btn_sz, corner_btn_sz, lang(VPI_LANG_EDIT_BTN), "edit.svg", VUI_BUTTON_STYLE_CORNER, layer, vpi_menu_main_edit_button_action, (void *) (intptr_t) layer);
    vui_button_create(vui, SCREEN_WIDTH-corner_btn_sz, 0, corner_btn_sz, corner_btn_sz, lang(VPI_LANG_SETTINGS_BTN), "settings.png", VUI_BUTTON_STYLE_CORNER, layer, vpi_menu_main_settings_button_action, (void *) (intptr_t) layer);

    // No entries text
    main_menu_no_console_lbl = vui_label_create(vui, 0, SCREEN_HEIGHT * 2 / 5, SCREEN_WIDTH, SCREEN_HEIGHT, lang(VPI_LANG_NO_CONSOLES_SYNCED), vui_color_create(0,0,0,0.5f), VUI_FONT_SIZE_SMALL, layer);

    // Update state of all buttons
    vpi_menu_main_populate(vui);

    if (!v) {
        vui_transition_fade_black_out(vui, NULL, NULL);
    } else {
        vui_transition_fade_layer_in(vui, layer, NULL, NULL);
    }
    IsSettings = 0;
}
