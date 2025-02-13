#include "menu_delete.h"

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "lang.h"
#include "menu_common.h"
#include "menu_edit.h"
#include "menu_main.h"
#include "ui/ui_anim.h"

static int fglayer;

void vpi_menu_delete_return_to_edit(vui_context_t *vui, int btn, void *v)
{
    vui_transition_fade_layer_out(vui, fglayer, vpi_menu_edit_return, v);
}

void vpi_menu_delete_return_to_main(vui_context_t *vui, int btn, void *v)
{
    int layer = (intptr_t) v;
    vui_transition_fade_layer_out(vui, layer, vpi_menu_main, (void *) (intptr_t) 1);
}

void vpi_menu_delete_status(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    int bglayer = vui_layer_create(vui);
    fglayer = vui_layer_create(vui);
    
    uint8_t console = (uintptr_t) v;

    vui_rect_t bkg_rect;
    int margin;
    vpi_menu_create_background(vui, bglayer, &bkg_rect, &margin);

    char cname[VPI_CONSOLE_MAX_NAME];
    strncpy(cname, vpi_config.connected_console_entries[console].name, sizeof(cname));

    vpi_config_remove_console(console);

    char buf[256];
    snprintf(buf, sizeof(buf), lang(VPI_LANG_DELETE_SUCCESS), cname);
    vui_label_create(vui, bkg_rect.x, bkg_rect.y + bkg_rect.h*1/5, bkg_rect.w, bkg_rect.h, buf, vui_color_create(1,1,1,1), VUI_FONT_SIZE_NORMAL, fglayer);

    printf("%s\n", buf);

    static const int ok_btn_w = BTN_SZ * 3;
    vui_button_create(vui, bkg_rect.x + bkg_rect.w/2 - (ok_btn_w)/2, bkg_rect.y + bkg_rect.h * 3 / 4, ok_btn_w, BTN_SZ, lang(VPI_LANG_OK_BTN), 0, VUI_BUTTON_STYLE_BUTTON, fglayer, vpi_menu_delete_return_to_main, v);

    vui_transition_fade_layer_in(vui, fglayer, 0, 0);
}

void vpi_menu_delete_ok(vui_context_t *vui, int btn, void *v)
{
    vui_transition_fade_layer_out(vui, fglayer, vpi_menu_delete_status, v);
}

void vpi_menu_delete(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    int bglayer = vui_layer_create(vui);
    fglayer = vui_layer_create(vui);

    vui_rect_t bkg_rect;
    int margin;
    vpi_menu_create_background(vui, bglayer, &bkg_rect, &margin);

    uint8_t console = (uintptr_t) v;

    char buf[256];
    snprintf(buf, sizeof(buf), lang(VPI_LANG_DELETE_CONFIRM), vpi_config.connected_console_entries[console].name);
    vui_label_create(vui, bkg_rect.x, bkg_rect.y + bkg_rect.h*1/5, bkg_rect.w, bkg_rect.h, buf, vui_color_create(1,1,1,1), VUI_FONT_SIZE_NORMAL, fglayer);

    static const int ok_btn_w = BTN_SZ * 3;
    static const int cancel_btn_w = BTN_SZ * 3;

    vui_button_create(vui, bkg_rect.x + bkg_rect.w/2 - (ok_btn_w + cancel_btn_w)/2, bkg_rect.y + bkg_rect.h * 3 / 4, ok_btn_w, BTN_SZ, lang(VPI_LANG_OK_BTN), 0, VUI_BUTTON_STYLE_BUTTON, fglayer, vpi_menu_delete_ok, v);
    vui_button_create(vui, bkg_rect.x + bkg_rect.w/2 - (ok_btn_w + cancel_btn_w)/2 + ok_btn_w, bkg_rect.y + bkg_rect.h * 3 / 4, cancel_btn_w, BTN_SZ, lang(VPI_LANG_CANCEL_BTN), 0, VUI_BUTTON_STYLE_BUTTON, fglayer, vpi_menu_delete_return_to_edit, (void *) (intptr_t) bglayer);

    vui_transition_fade_layer_in(vui, fglayer, 0, 0);
}