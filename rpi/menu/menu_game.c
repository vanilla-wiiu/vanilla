#include "menu_game.h"

#include <stdio.h>
#include <vanilla.h>

#include "config.h"
#include "lang.h"
#include "game/game_main.h"
#include "menu_common.h"
#include "menu_main.h"
#include "ui/ui_anim.h"

void back_to_main_menu(vui_context_t *vui, int button, void *v)
{
    int layer = (intptr_t) v;
    vui_transition_fade_layer_out(vui, layer, vpi_menu_main, (void *) (intptr_t) 1);
}

void show_error(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    vui_enable_background(vui, 1);

    int scrw, scrh;
    vui_get_screen_size(vui, &scrw, &scrh);

    int layer = vui_layer_create(vui);

    vui_label_create(vui, 0, scrh * 1 / 6, scrw, scrh, lang(VPI_LANG_ERROR), vui_color_create(0.25f,0.25f,0.25f,1), VUI_FONT_SIZE_NORMAL, layer);
    
    char buf[10];
    snprintf(buf, sizeof(buf), "%i", (int) (intptr_t) v);
    vui_label_create(vui, 0, scrh * 3 / 6, scrw, scrh, buf, vui_color_create(1,0,0,1), VUI_FONT_SIZE_NORMAL, layer);

    const int ok_btn_w = BTN_SZ*2;
    vui_button_create(vui, scrw/2 - ok_btn_w/2, scrh * 3 / 4, ok_btn_w, BTN_SZ, lang(VPI_LANG_OK_BTN), 0, VUI_BUTTON_STYLE_BUTTON, layer, back_to_main_menu, (void *) (intptr_t) layer);

    vui_transition_fade_black_out(vui, 0, 0);
}

void vpi_display_update(vui_context_t *vui, int64_t time, void *v)
{
    int backend_err = vpi_game_error();
    if (backend_err != VANILLA_SUCCESS) {
        vui_game_mode_set(vui, 0);
        show_error(vui, (void*)(intptr_t) backend_err);
        return;
    }
}

void vpi_menu_game(vui_context_t *vui, void *v)
{
    int console = (intptr_t) v;

    vui_reset(vui);

    vui_enable_background(vui, 0);

    vpi_console_entry_t *entry = vpi_config.connected_console_entries + console;
    int r = vanilla_start(vpi_config.server_address, entry->bssid, entry->psk);
    if (r != VANILLA_SUCCESS) {
        show_error(vui, (void*)(intptr_t) r);
    } else {
        char buf[100];
        snprintf(buf, sizeof(buf), lang(VPI_LANG_CONNECTING_TO), entry->name);

        int fglayer = vui_layer_create(vui);

        int scrw, scrh;
        vui_get_screen_size(vui, &scrw, &scrh);
        vui_label_create(vui, 0, scrh * 2 / 5, scrw, scrh, buf, vui_color_create(0.5f,0.5f,0.5f,1), VUI_FONT_SIZE_NORMAL, fglayer);

        vui_transition_fade_layer_in(vui, fglayer, 0, 0);

        vpi_game_start(vui);

        vui_game_mode_set(vui, 1);
        vui_start_passive_animation(vui, vpi_display_update, 0);
    }
}