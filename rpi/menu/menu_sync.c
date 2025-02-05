#include "menu_sync.h"

#include "menu_main.h"
#include "ui/ui_anim.h"

void vanilla_sync_menu_back_action(vui_context_t *vui, int btn, void *v)
{
    vui_transition_fade_layer_out(vui, (int) (intptr_t) v, vanilla_menu_main, 0);
}

void vanilla_menu_sync(vui_context_t *vui, void *d)
{
    // Clears all extra layers
    vui_reset(vui);

    // int bglayer = vui_layer_create(vui);
    int layer = vui_layer_create(vui);

    int scrw, scrh;
    vui_get_screen_size(vui, &scrw, &scrh);

    const int margin = scrw/75;
    vui_rect_t bkg_rect;
    bkg_rect.x = margin;
    bkg_rect.y = margin;
    bkg_rect.w = scrw-margin-margin;
    bkg_rect.h = scrh-margin-margin;

    vui_rect_create(vui, bkg_rect.x, bkg_rect.y, bkg_rect.w, bkg_rect.h, scrh*2/10, vui_color_create(0, 0, 0, 0.66f), layer);

    const int lbl_margin = margin * 3;
    vui_label_create(vui, bkg_rect.x + lbl_margin, bkg_rect.y + lbl_margin, bkg_rect.w - lbl_margin - lbl_margin, bkg_rect.h - lbl_margin - lbl_margin, "Touch the symbols in the order they are displayed on the TV screen from left to right.", vui_color_create(1,1,1,1), layer);

    int back_btn_h = scrh/6;
    vui_button_create(vui, 0, scrh-back_btn_h, scrw/4, back_btn_h, "Cancel", VUI_BUTTON_STYLE_CORNER, layer, vanilla_sync_menu_back_action, (void *) (intptr_t) layer);

    vui_transition_fade_layer_in(vui, layer, 0, 0);
}