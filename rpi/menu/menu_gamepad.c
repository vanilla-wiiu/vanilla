#include "menu_gamepad.h"

#include "lang.h"
#include "menu/menu_common.h"
#include "menu/menu_settings.h"
#include "ui/ui_anim.h"

static void return_to_settings(vui_context_t *vui, int btn, void *v)
{
    int layer = (intptr_t) v;
    vui_transition_fade_layer_out(vui, layer, vpi_menu_settings, 0);
}

void vpi_menu_gamepad(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    int layer = vui_layer_create(vui);

    int scrw, scrh;
    vui_get_screen_size(vui, &scrw, &scrh);

    int img_w = scrw / 2;
    int img_h = scrh / 2;
    vui_image_create(vui, scrw/2-img_w/2, scrh/2-img_h/2 + 50, img_w, img_h, "gamepad.svg", layer);

    int tmp_lbl = vui_label_create(vui, scrw/4, scrh/8, scrw/2, scrh, "Controller remapping is not yet implemented in this version, please come back soon...", vui_color_create(0.25f,0.25f,0.25f,1), VUI_FONT_SIZE_SMALL, layer);

    // Back button
    vpi_menu_create_back_button(vui, layer, return_to_settings, (void *) (intptr_t) layer);

    // More button
    // vui_button_create(vui, scrw-BTN_SZ, 0, BTN_SZ, BTN_SZ, lang(VPI_LANG_MORE), 0, VUI_BUTTON_STYLE_CORNER, layer, return_to_settings, (void *) (intptr_t) layer);

    vui_transition_fade_layer_in(vui, layer, 0, 0);
}