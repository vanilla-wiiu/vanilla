#include "menu_connection.h"

#include "config.h"
#include "lang.h"
#include "menu_common.h"
#include "menu_settings.h"
#include "ui/ui_anim.h"

// The Wii U only supports the first 3 regions, so we ignore the rest of them
#define MAX_REGIONS 3

static int bglayer;
static int fglayer;
static int region_btns[MAX_REGIONS];

static void return_to_settings(vui_context_t *vui, int btn, void *v)
{
    vui_transition_fade_layer_out(vui, bglayer, vpi_menu_settings, 0);
}

static void region_clicked(vui_context_t *vui, int btn, void *v)
{
    int reg = (intptr_t) v;

    vpi_config.region = reg;
    vpi_config_save();

    for (int i = 0; i < MAX_REGIONS; i++) {
        int b = region_btns[i];
        vui_button_update_checked(vui, b, vpi_config.region == i);
    }

    return_to_settings(vui, btn, 0);
}

void vpi_menu_region(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    bglayer = vui_layer_create(vui);
    fglayer = vui_layer_create(vui);

    vui_rect_t bkg_rect;
    int margin;
    vpi_menu_create_background(vui, bglayer, &bkg_rect, &margin);

    const int lbl_margin = margin * 2;
    const int lbl_y = bkg_rect.y + lbl_margin;
    vui_label_create(vui, bkg_rect.x + lbl_margin, lbl_y, bkg_rect.w - lbl_margin - lbl_margin, bkg_rect.h - lbl_margin - lbl_margin, lang(VPI_LANG_REGION_HELP_1), vui_color_create(1,1,1,1), VUI_FONT_SIZE_NORMAL, fglayer);

    const int lbl2_width = bkg_rect.w*2/3;
    vui_label_create(vui, bkg_rect.x + bkg_rect.w/2 - lbl2_width/2, lbl_y + bkg_rect.h/8, lbl2_width, bkg_rect.h - lbl_margin - lbl_margin, lang(VPI_LANG_REGION_HELP_2), vui_color_create(0.66f,0.66f,0.66f,1), VUI_FONT_SIZE_SMALL, fglayer);

    const int btn_w = bkg_rect.w/2;
    const int btn_y = bkg_rect.y + bkg_rect.h * 13 / 32;
    const int btn_x = bkg_rect.x + bkg_rect.w/2 - btn_w/2;

    for (int i = 0; i < MAX_REGIONS; i++) {
        int b = vui_button_create(vui, btn_x, btn_y + BTN_SZ * i, btn_w, BTN_SZ, lang(VPI_LANG_REGION_JAPAN + i), 0, VUI_BUTTON_STYLE_BUTTON, fglayer, region_clicked, (void *) (intptr_t) i);
        vui_button_update_checkable(vui, b, 1);
        vui_button_update_checked(vui, b, vpi_config.region == i);
        region_btns[i] = b;
    }

    // Back button
    vpi_menu_create_back_button(vui, fglayer, return_to_settings, (void *) (intptr_t) bglayer);
    
    vui_transition_fade_layer_in(vui, bglayer, 0, 0);
}