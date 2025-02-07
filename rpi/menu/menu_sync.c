#include "menu_sync.h"

#include <stdio.h>
#include <stdint.h>

#include "menu_common.h"
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

void start_syncing(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    int bglayer = vui_layer_create(vui);
    int fglayer = vui_layer_create(vui);

    int scrw, scrh;
    vui_get_screen_size(vui, &scrw, &scrh);

    const int margin = scrw/75;
    vui_rect_t bkg_rect;
    bkg_rect.x = margin;
    bkg_rect.y = margin;
    bkg_rect.w = scrw-margin-margin;
    bkg_rect.h = scrh-margin-margin;
    
    vui_rect_create(vui, bkg_rect.x, bkg_rect.y, bkg_rect.w, bkg_rect.h, scrh*2/10, vui_color_create(0, 0, 0, 0.66f), bglayer);

    vui_label_create(vui, bkg_rect.x, bkg_rect.y + bkg_rect.h/4, bkg_rect.w, bkg_rect.h, "Connecting to the Wii U console...", vui_color_create(1,1,1,1), VUI_FONT_SIZE_NORMAL, fglayer);

    vui_transition_fade_layer_in(vui, fglayer, 0, 0);
}

void transition_to_sync_window(vui_context_t *vui, void *v)
{
    vui_transition_fade_layer_out(vui, sync_fglayer, start_syncing, 0);
}

void vanilla_sync_menu_back_action(vui_context_t *vui, int btn, void *v)
{
    vui_transition_fade_layer_out(vui, (int) (intptr_t) v, vanilla_menu_main, 0);
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
}

void create_sync_btn(vui_context_t *vui, int x, int data, int layer, int origin_x, int origin_y)
{
    int btn = vui_button_create(vui, origin_x + BTN_SZ * x, origin_y, BTN_SZ, BTN_SZ, 0, suit_icons_black[data], VUI_BUTTON_STYLE_BUTTON, layer, sync_btn_clicked, 0);
    sync_btns[data] = btn;
}

void vanilla_menu_sync(vui_context_t *vui, void *d)
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

    int scrw, scrh;
    vui_get_screen_size(vui, &scrw, &scrh);

    const int margin = scrw/75;
    vui_rect_t bkg_rect;
    bkg_rect.x = margin;
    bkg_rect.y = margin;
    bkg_rect.w = scrw-margin-margin;
    bkg_rect.h = scrh-margin-margin;

    vui_rect_create(vui, bkg_rect.x, bkg_rect.y, bkg_rect.w, bkg_rect.h, scrh*2/10, vui_color_create(0, 0, 0, 0.66f), sync_bglayer);

    const int lbl_margin = margin * 3;
    vui_label_create(vui, bkg_rect.x + lbl_margin, bkg_rect.y + lbl_margin, bkg_rect.w - lbl_margin - lbl_margin, bkg_rect.h - lbl_margin - lbl_margin, "Touch the symbols in the order they are displayed on the TV screen from left to right.", vui_color_create(1,1,1,1), VUI_FONT_SIZE_NORMAL, sync_fglayer);

    const int lbl2_width = scrw/2;
    vui_label_create(vui, scrw/2 - lbl2_width/2, bkg_rect.y + lbl_margin + scrh/5, lbl2_width, bkg_rect.h - lbl_margin - lbl_margin, "If the symbols are not displayed on the TV screen, press the SYNC Button on the Wii U console.", vui_color_create(0.66f,0.66f,0.66f,1), VUI_FONT_SIZE_SMALL, sync_fglayer);

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
    vui_button_create(vui, sync_btn_x + BTN_SZ * 3, sync_entry_rect.y, BTN_SZ, sync_entry_rect.h, 0, "backspace_black.svg", VUI_BUTTON_STYLE_BUTTON, sync_fglayer, bksp_btn_clicked, 0);

    int back_btn_h = scrh/6;
    vui_button_create(vui, 0, scrh-back_btn_h, scrw/4, back_btn_h, "Cancel", 0, VUI_BUTTON_STYLE_CORNER, sync_fglayer, vanilla_sync_menu_back_action, (void *) (intptr_t) sync_bglayer);

    vui_transition_fade_layer_in(vui, sync_bglayer, 0, 0);
}