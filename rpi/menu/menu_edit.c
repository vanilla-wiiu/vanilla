#include "menu_edit.h"

#include "config.h"
#include "lang.h"
#include "menu_common.h"
#include "menu_delete.h"
#include "menu_rename.h"
#include "menu_main.h"
#include "ui/ui_anim.h"

static int bglayer;
static int fglayer;

static void delete_btn_pressed(vui_context_t *vui, int btn, void *v)
{
    vui_transition_fade_layer_out(vui, fglayer, vpi_menu_delete, v);
}

static void rename_btn_pressed(vui_context_t *vui, int btn, void *v)
{
    vui_transition_fade_layer_out(vui, fglayer, vpi_menu_rename, v);
}

static void return_to_main(vui_context_t *vui, int btn, void *v)
{
    int layer = (intptr_t) v;
    vui_transition_fade_layer_out(vui, layer, vpi_menu_main, (void *) (intptr_t) 1);
}

void vpi_menu_edit_nofade(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    bglayer = vui_layer_create(vui);
    fglayer = vui_layer_create(vui);

    vui_rect_t bkg_rect;
    int margin;
    vpi_menu_create_background(vui, bglayer, &bkg_rect, &margin);

    uint8_t console = (uintptr_t) v;
    
    vui_label_create(vui, bkg_rect.x, bkg_rect.y + bkg_rect.h*1/5, bkg_rect.w, bkg_rect.h, vpi_config.connected_console_entries[console].name, vui_color_create(1,1,1,1), VUI_FONT_SIZE_NORMAL, fglayer);

    const int btn_w = bkg_rect.w / 2;
    const int btn_y = bkg_rect.y + bkg_rect.h * 1 / 2;
    vui_button_create(vui, bkg_rect.x + bkg_rect.w/2 - btn_w/2, btn_y + BTN_SZ*0, btn_w, BTN_SZ, lang(VPI_LANG_RENAME), 0, VUI_BUTTON_STYLE_BUTTON, fglayer, rename_btn_pressed, v);
    vui_button_create(vui, bkg_rect.x + bkg_rect.w/2 - btn_w/2, btn_y + BTN_SZ*1, btn_w, BTN_SZ, lang(VPI_LANG_DELETE), 0, VUI_BUTTON_STYLE_BUTTON, fglayer, delete_btn_pressed, v);

    // Back button
    vpi_menu_create_back_button(vui, fglayer, return_to_main, (void *) (intptr_t) bglayer);
}

void vpi_menu_edit(vui_context_t *vui, void *v)
{
    vpi_menu_edit_nofade(vui, v);

    vui_transition_fade_layer_in(vui, bglayer, 0, 0);
}

void vpi_menu_edit_return(vui_context_t *vui, void *v)
{
    vpi_menu_edit_nofade(vui, v);

    vui_transition_fade_layer_in(vui, fglayer, 0, 0);
}