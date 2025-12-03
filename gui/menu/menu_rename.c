#include "menu_rename.h"

static int fglayer;

#include "config.h"
#include "lang.h"
#include "menu_common.h"
#include "menu_edit.h"
#include "ui/ui_anim.h"

static int textedit;

static void vpi_menu_rename_return_to_edit(vui_context_t *vui, int btn, void *v)
{
    vui_transition_fade_layer_out(vui, fglayer, vpi_menu_edit_return, v);
}

static void commit_rename(vui_context_t *vui, int btn, void *v)
{
    uint8_t console = (uintptr_t) v;

    char buf[VPI_CONSOLE_MAX_NAME];
    vui_textedit_get_text(vui, textedit, buf, sizeof(buf));
    vpi_config_rename_console(console, buf);

    vpi_menu_rename_return_to_edit(vui, btn, v);
}

void vpi_menu_rename(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    int bglayer = vui_layer_create(vui);
    fglayer = vui_layer_create(vui);

    vui_rect_t bkg_rect;
    int margin;
    vpi_menu_create_background(vui, bglayer, &bkg_rect, &margin);

    uint8_t console = (uintptr_t) v;

    const int textedit_w = bkg_rect.w / 2;
    const int textedit_h = vui_get_font_height(vui, VUI_FONT_SIZE_NORMAL);
    textedit = vui_textedit_create(vui, bkg_rect.x + bkg_rect.w/2 - textedit_w/2, bkg_rect.y + bkg_rect.h * 1 / 3, textedit_w, textedit_h, vpi_config.connected_console_entries[console].name, VUI_FONT_SIZE_NORMAL, 0, fglayer);

    static const int ok_btn_w = BTN_SZ * 3;
    static const int cancel_btn_w = BTN_SZ * 3;

    vui_button_create(vui, bkg_rect.x + bkg_rect.w/2 - (ok_btn_w + cancel_btn_w)/2, bkg_rect.y + bkg_rect.h * 3 / 4, ok_btn_w, BTN_SZ, lang(VPI_LANG_OK_BTN), 0, VUI_BUTTON_STYLE_BUTTON, fglayer, commit_rename, v);
    vui_button_create(vui, bkg_rect.x + bkg_rect.w/2 - (ok_btn_w + cancel_btn_w)/2 + ok_btn_w, bkg_rect.y + bkg_rect.h * 3 / 4, cancel_btn_w, BTN_SZ, lang(VPI_LANG_CANCEL_BTN), 0, VUI_BUTTON_STYLE_BUTTON, fglayer, vpi_menu_rename_return_to_edit, v);

    vui_transition_fade_layer_in(vui, fglayer, 0, 0);
}
