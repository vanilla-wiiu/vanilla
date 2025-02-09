#include "menu_sudo.h"

#include "menu_common.h"
#include "lang.h"
#include "ui/ui_anim.h"

static int sudo_fglayer;
static vui_callback_t sudo_success;
static void *sudo_success_data;
static vui_callback_t sudo_cancel;
static void *sudo_cancel_data;

void cancel_sudo(vui_context_t *vui, int button, void *data)
{
    vui_transition_fade_layer_out(vui, sudo_fglayer, sudo_cancel, sudo_cancel_data);
}

void ok_sudo(vui_context_t *vui, int button, void *data)
{
    vui_transition_fade_layer_out(vui, sudo_fglayer, sudo_success, sudo_success_data);
}

void vpi_menu_sudo(vui_context_t *vui, vui_callback_t success, void *success_data, vui_callback_t cancel, void *cancel_data)
{
    vui_reset(vui);

    sudo_success = success;
    sudo_success_data = success_data;
    sudo_cancel = cancel;
    sudo_cancel_data = cancel_data;

    int bglayer = vui_layer_create(vui);
    sudo_fglayer = vui_layer_create(vui);

    vui_rect_t bkg_rect;
    int margin;
    vpi_menu_create_background(vui, bglayer, &bkg_rect, &margin);

    int scrw, scrh;
    vui_get_screen_size(vui, &scrw, &scrh);

    const int lbl_margin = margin * 3;
    vui_label_create(vui, bkg_rect.x + lbl_margin, bkg_rect.y + lbl_margin, bkg_rect.w - lbl_margin - lbl_margin, bkg_rect.h - lbl_margin - lbl_margin, lang(VPI_LANG_SUDO_HELP_1), vui_color_create(1,1,1,1), VUI_FONT_SIZE_NORMAL, sudo_fglayer);

    const int lbl2_width = scrw/2;
    vui_label_create(vui, scrw/2 - lbl2_width/2, bkg_rect.y + lbl_margin + scrh/5, lbl2_width, bkg_rect.h - lbl_margin - lbl_margin, lang(VPI_LANG_SUDO_HELP_2), vui_color_create(0.66f,0.66f,0.66f,1), VUI_FONT_SIZE_SMALL, sudo_fglayer);

    static const int ok_btn_w = BTN_SZ * 3;
    static const int cancel_btn_w = BTN_SZ * 3;

    vui_button_create(vui, bkg_rect.x + bkg_rect.w/2 - (ok_btn_w + cancel_btn_w)/2, bkg_rect.y + bkg_rect.h * 3 / 4, ok_btn_w, BTN_SZ, lang(VPI_LANG_OK_BTN), 0, VUI_BUTTON_STYLE_BUTTON, sudo_fglayer, ok_sudo, 0);
    vui_button_create(vui, bkg_rect.x + bkg_rect.w/2 - (ok_btn_w + cancel_btn_w)/2 + ok_btn_w, bkg_rect.y + bkg_rect.h * 3 / 4, cancel_btn_w, BTN_SZ, lang(VPI_LANG_CANCEL_BTN), 0, VUI_BUTTON_STYLE_BUTTON, sudo_fglayer, cancel_sudo, 0);

    vui_transition_fade_layer_in(vui, sudo_fglayer, 0, 0);
}