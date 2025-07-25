#include "menu_sync.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vanilla.h>

#include "config.h"
#include "lang.h"
#include "menu_common.h"
#include "menu_game.h"
#include "menu_main.h"
#include "ui/ui_anim.h"
#include "ui/ui_util.h"

#define SYNC_BTN_COUNT 4
static int sync_btns[SYNC_BTN_COUNT];
static uint8_t sync_str[SYNC_BTN_COUNT];
static uint8_t sync_str_len = 0;
static vui_rect_t sync_entry_rect;
static int sync_entry_images[SYNC_BTN_COUNT];
static int sync_bglayer;
static int sync_fglayer;
static int sync_cancel_btn;
static int sync_bksp_btn;

static const char *suit_icons_black[4] = {
    "spade_black.svg",
    "heart_black.svg",
    "diamond_black.svg",
    "club_black.svg",
};

static const char *suit_icons_white[4] = {
    "spade_white.svg",
    "heart_white.svg",
    "diamond_white.svg",
    "club_white.svg",
};

void vpi_sync_menu_back_action(vui_context_t *vui, int btn, void *v)
{
    vui_transition_fade_layer_out(vui, (int) (intptr_t) v, vpi_menu_main, (void *) (intptr_t) 1);
}

void sync_return_to_main(vui_context_t *vui, int button, void *data)
{
    vui_transition_fade_layer_out(vui, sync_fglayer, vpi_menu_sync, (void *) (intptr_t) 1);
}

void vpi_sync_show_error(vui_context_t *vui, void *data)
{
    sync_fglayer = vpi_menu_show_error(vui, (intptr_t) data, 1, sync_return_to_main, 0);
}

int intpow(int x, unsigned int p)
{
    if (p == 0) return 1;
    if (p == 1) return x;

    int tmp = intpow(x, p/2);
    if (p%2 == 0) return tmp * tmp;
    else return x * tmp * tmp;
}

void sync_animation_step(vui_context_t *ctx, int64_t time, void *userdata)
{
    int progress_lbl = (int) (intptr_t) userdata;
    
    static const int utf8_char_len = 3; // Somewhat hacky, but the important thing is that it works
    static const int progress_txt_len = 9;
    static const size_t progress_txt_sz = progress_txt_len * utf8_char_len + 1;

    char progress_txt[progress_txt_sz];
    progress_txt[progress_txt_sz-1] = 0;

    int64_t m = int64min((sin(time * 0.000004) * 0.5 + 0.5) * progress_txt_len, progress_txt_len - 1);
    for (int i = 0; i < progress_txt_len; i++) {
        memcpy(progress_txt + utf8_char_len * i, (i == m) ? "\xE2\xAC\xA4" : "\xE2\x80\xA2", utf8_char_len);
    }

    vui_label_update_text(ctx, progress_lbl, progress_txt);

    // Poll Vanilla for sync status
    vanilla_event_t event;
    while (vanilla_poll_event(&event)) {
        if (event.type == VANILLA_EVENT_ERROR) {
            int err = * (int *) event.data;

            // Transition to error screen showing error code
            vui_transition_fade_layer_out(ctx, sync_fglayer, vpi_sync_show_error, (void *) (intptr_t) err);

            vanilla_free_event(&event);
            vanilla_stop();
        } else if (event.type == VANILLA_EVENT_SYNC) {
            vanilla_sync_event_t *sync = (vanilla_sync_event_t *) event.data;

            // Save this as a new console and transition to success screen
            int found = -1;

            for (uint8_t i = 0; i < vpi_config.connected_console_count; i++) {
                if (!memcmp(vpi_config.connected_console_entries[i].bssid.bssid, sync->data.bssid.bssid, sizeof(vanilla_bssid_t))) {
                    // We don't bother checking PSK because BSSID is already unique and the PSK should not have changed
                    found = i;
                    break;
                }
            }

            if (found == -1) {
                vpi_console_entry_t console;
                snprintf(console.name, sizeof(console.name), "Wii U %i", vpi_config.connected_console_count + 1);
                console.bssid = sync->data.bssid;
                console.psk = sync->data.psk;
                found = vpi_config_add_console(&console);
            }

            vui_transition_fade_layer_out(ctx, sync_fglayer, vpi_menu_game, (void *) (intptr_t) found);

            vanilla_free_event(&event);
            vanilla_stop();
        } else {
            vanilla_free_event(&event);
        }
    }
}

void cancel_sync(vui_context_t *vui, int button, void *v)
{
    vanilla_stop();
    sync_return_to_main(vui, button, 0);
}

void start_syncing(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    int bglayer = vui_layer_create(vui);
    sync_fglayer = vui_layer_create(vui);

    vui_rect_t bkg_rect;
    int margin;
    vpi_menu_create_background(vui, bglayer, &bkg_rect, &margin);

    vui_label_create(vui, bkg_rect.x, bkg_rect.y + bkg_rect.h*1/5, bkg_rect.w, bkg_rect.h, lang(VPI_LANG_SYNC_CONNECTING), vui_color_create(1,1,1,1), VUI_FONT_SIZE_NORMAL, sync_fglayer);

    uint16_t code = 0;
    for (int i = 0; i < SYNC_BTN_COUNT; i++) {
        code += sync_str[i] * intpow(10, SYNC_BTN_COUNT - 1 - i);
    }

    int progress_lbl = vui_label_create(vui, bkg_rect.x, bkg_rect.y + bkg_rect.h*2/5, bkg_rect.w, bkg_rect.h, "", vui_color_create(1,1,1,1), VUI_FONT_SIZE_NORMAL, sync_fglayer);
    sync_animation_step(vui, 0, (void *) (intptr_t) progress_lbl);

    vui_button_create(vui, bkg_rect.x + bkg_rect.w/2 - BTN_SZ*3/2, bkg_rect.y + bkg_rect.h * 3 / 4, BTN_SZ*3, BTN_SZ, lang(VPI_LANG_CANCEL_BTN), 0, VUI_BUTTON_STYLE_BUTTON, sync_fglayer, cancel_sync, 0);

    int ret = vanilla_sync(code, vpi_config.server_address);
    if (ret == VANILLA_SUCCESS) {
        vui_transition_fade_layer_in(vui, sync_fglayer, 0, 0);
        vui_start_passive_animation(vui, sync_animation_step, (void *) (intptr_t) progress_lbl);
    } else {
        vpi_sync_show_error(vui, (void *) (intptr_t) ret);
    }
}

void transition_to_sync_window(vui_context_t *vui, void *v)
{
    vui_transition_fade_layer_out(vui, sync_fglayer, start_syncing, 0);
}

void sync_btn_clicked(vui_context_t *vui, int button, void *data)
{
    // Prevent overflowing string
    if (sync_str_len == SYNC_BTN_COUNT) {
        return;
    }

    // Determine which digit to append
    int append = -1;
    for (int i = 0; i < SYNC_BTN_COUNT; i++) {
        if (button == sync_btns[i]) {
            append = i;
            break;
        }
    }

    // Add char to string
    if (append != -1) {
        sync_str[sync_str_len] = append;
        
        const int icon_space = sync_entry_rect.w/4;
        const int icon_w = intmin(icon_space, sync_entry_rect.h);
        sync_entry_images[sync_str_len] = vui_image_create(vui, sync_entry_rect.x + icon_space * sync_str_len + icon_space/2 - icon_w/2, sync_entry_rect.y + sync_entry_rect.h / 2 - icon_w/2, icon_w, icon_w, suit_icons_white[sync_str[sync_str_len]], sync_fglayer);
        
        sync_str_len++;

        if (sync_str_len == SYNC_BTN_COUNT) {
            vui_timer(vui, 250000, transition_to_sync_window, 0);
        }

        vui_button_set_cancel(vui, sync_bksp_btn);
    }
}

void bksp_btn_clicked(vui_context_t *vui, int button, void *data)
{
    // Prevent underflowing string
    if (sync_str_len == 0) {
        return;
    }

    sync_str_len--;
    vui_image_destroy(vui, sync_entry_images[sync_str_len]);
    sync_entry_images[sync_str_len] = -1;

    if (sync_str_len == 0) {
        vui_button_set_cancel(vui, sync_cancel_btn);
    }
}

void create_sync_btn(vui_context_t *vui, int x, int data, int layer, int origin_x, int origin_y)
{
    int btn = vui_button_create(vui, origin_x + BTN_SZ * x, origin_y, BTN_SZ, BTN_SZ, 0, suit_icons_black[data], VUI_BUTTON_STYLE_BUTTON, layer, sync_btn_clicked, 0);
    sync_btns[data] = btn;
}

void vpi_menu_sync_start(vui_context_t *vui, void *d)
{
    // Clears all extra layers
    vui_reset(vui);

    for (int i = 0; i < SYNC_BTN_COUNT; i++) {
        sync_entry_images[i] = -1;
    }
    sync_str_len = 0;

    // int bglayer = vui_layer_create(vui);
    sync_bglayer = vui_layer_create(vui);
    sync_fglayer = vui_layer_create(vui);

    vui_rect_t bkg_rect;
    int margin;
    vpi_menu_create_background(vui, sync_bglayer, &bkg_rect, &margin);

    int scrw, scrh;
    vui_get_screen_size(vui, &scrw, &scrh);

    const int lbl_margin = margin * 3;
    vui_label_create(vui, bkg_rect.x + lbl_margin, bkg_rect.y + lbl_margin, bkg_rect.w - lbl_margin - lbl_margin, bkg_rect.h - lbl_margin - lbl_margin, lang(VPI_LANG_SYNC_HELP_1), vui_color_create(1,1,1,1), VUI_FONT_SIZE_NORMAL, sync_fglayer);

    const int lbl2_width = scrw/2;
    vui_label_create(vui, scrw/2 - lbl2_width/2, bkg_rect.y + lbl_margin + scrh/5, lbl2_width, bkg_rect.h - lbl_margin - lbl_margin, lang(VPI_LANG_SYNC_HELP_2), vui_color_create(0.66f,0.66f,0.66f,1), VUI_FONT_SIZE_SMALL, sync_fglayer);

    // Create 4 symbol buttons
    const int sync_btn_x = scrw/2 - (BTN_SZ*SYNC_BTN_COUNT)/2;
    const int sync_btn_y = scrh - BTN_SZ - margin * 3;

    // Gamepad swaps 1 and 2 for some reason (probably obfuscation)
    create_sync_btn(vui, 0, 0, sync_fglayer, sync_btn_x, sync_btn_y);
    create_sync_btn(vui, 1, 2, sync_fglayer, sync_btn_x, sync_btn_y);
    create_sync_btn(vui, 2, 1, sync_fglayer, sync_btn_x, sync_btn_y);
    create_sync_btn(vui, 3, 3, sync_fglayer, sync_btn_x, sync_btn_y);

    const int entry_y = sync_btn_y - BTN_SZ;

    // Create entry rect
    sync_entry_rect.w = BTN_SZ * 3;
    sync_entry_rect.h = BTN_SZ * 3 / 4;
    sync_entry_rect.x = sync_btn_x;
    sync_entry_rect.y = entry_y + BTN_SZ/2 - sync_entry_rect.h/2;
    vui_rect_create(vui, sync_entry_rect.x, sync_entry_rect.y, sync_entry_rect.w, sync_entry_rect.h, 5, vui_color_create(0,0,0,0.66f), sync_fglayer);

    // Create backspace button
    sync_bksp_btn = vui_button_create(vui, sync_btn_x + BTN_SZ * 3, sync_entry_rect.y, BTN_SZ, sync_entry_rect.h, 0, "backspace_black.svg", VUI_BUTTON_STYLE_BUTTON, sync_fglayer, bksp_btn_clicked, 0);

    int back_btn_h = scrh/6;
    sync_cancel_btn = vui_button_create(vui, 0, scrh-back_btn_h, scrw/4, back_btn_h, lang(VPI_LANG_CANCEL_BTN), 0, VUI_BUTTON_STYLE_CORNER, sync_fglayer, vpi_sync_menu_back_action, (void *) (intptr_t) sync_bglayer);

    vui_transition_fade_layer_in(vui, d ? sync_fglayer : sync_bglayer, 0, 0);

    vui_button_set_cancel(vui, sync_cancel_btn);
}

void vpi_menu_sync(vui_context_t *vui, void *d)
{
    vpi_menu_start_pipe(vui, 0, vpi_menu_sync_start, d, 0, 0);
}
