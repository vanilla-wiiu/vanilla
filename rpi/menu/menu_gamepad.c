#include "menu_gamepad.h"

#include <vanilla.h>

#include "lang.h"
#include "menu/menu_common.h"
#include "menu/menu_settings.h"
#include "ui/ui_anim.h"

static int GAMEPAD_MAP_BTNS[VANILLA_BTN_COUNT];

static void return_to_settings(vui_context_t *vui, int btn, void *v)
{
    int layer = (intptr_t) v;
    vui_transition_fade_layer_out(vui, layer, vpi_menu_settings, 0);
}

static void create_gamepad_btn(vui_context_t *vui, int index, const char *text, const char *icon, int x, int y, int w, int h, int layer)
{
    int b = vui_button_create(vui, x, y, w, h, text, icon, VUI_BUTTON_STYLE_BUTTON, layer, 0, 0);
    vui_button_update_font_size(vui, b, VUI_FONT_SIZE_SMALL);
    GAMEPAD_MAP_BTNS[index] = b;
}

void vpi_menu_gamepad(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    int layer = vui_layer_create(vui);

    int scrw, scrh;
    vui_get_screen_size(vui, &scrw, &scrh);

    int img_w = scrw / 2;
    int img_h = scrh / 2;
    vui_image_create(vui, scrw/2-img_w/2, scrh/2-img_h/2, img_w, img_h, "gamepad.svg", layer);

    // int tmp_lbl = vui_label_create(vui, scrw/4, scrh/8, scrw/2, scrh, "Controller remapping is not yet implemented in this version, please come back soon...", vui_color_create(0.25f,0.25f,0.25f,1), VUI_FONT_SIZE_SMALL, layer);

    // Back button
    vpi_menu_create_back_button(vui, layer, return_to_settings, (void *) (intptr_t) layer);

    // Create gamepad buttons
    const int CBTN_SZ = BTN_SZ*3/4;
    
    // Left stick
    const int STICK_Y = BTN_SZ;
    create_gamepad_btn(vui, VANILLA_AXIS_L_UP, 0, "up.svg", CBTN_SZ, STICK_Y + 0, CBTN_SZ, CBTN_SZ, layer);
    create_gamepad_btn(vui, VANILLA_AXIS_L_LEFT, 0, "left.svg", 0, STICK_Y + CBTN_SZ, CBTN_SZ, CBTN_SZ, layer);
    create_gamepad_btn(vui, VANILLA_BTN_L3, 0, 0, CBTN_SZ, STICK_Y + CBTN_SZ, CBTN_SZ, CBTN_SZ, layer);
    create_gamepad_btn(vui, VANILLA_AXIS_L_RIGHT, 0, "right.svg", CBTN_SZ + CBTN_SZ, STICK_Y + CBTN_SZ, CBTN_SZ, CBTN_SZ, layer);
    create_gamepad_btn(vui, VANILLA_AXIS_L_DOWN, 0, "down.svg", CBTN_SZ, STICK_Y + CBTN_SZ + CBTN_SZ, CBTN_SZ, CBTN_SZ, layer);

    // D-pad
    const int DPAD_Y = scrh - CBTN_SZ * 3;
    create_gamepad_btn(vui, VANILLA_BTN_UP, "|", 0, CBTN_SZ, DPAD_Y + 0, CBTN_SZ, CBTN_SZ, layer);
    create_gamepad_btn(vui, VANILLA_BTN_LEFT, "—", 0, 0, DPAD_Y + CBTN_SZ, CBTN_SZ, CBTN_SZ, layer);
    create_gamepad_btn(vui, VANILLA_BTN_RIGHT, "—", 0, CBTN_SZ + CBTN_SZ, DPAD_Y + CBTN_SZ, CBTN_SZ, CBTN_SZ, layer);
    create_gamepad_btn(vui, VANILLA_BTN_DOWN, "|", 0, CBTN_SZ, DPAD_Y + CBTN_SZ + CBTN_SZ, CBTN_SZ, CBTN_SZ, layer);
    
    // Right stick
    const int RSTICK_X = scrw - CBTN_SZ*3;
    create_gamepad_btn(vui, VANILLA_AXIS_R_UP, 0, "up.svg", RSTICK_X + CBTN_SZ, STICK_Y + 0, CBTN_SZ, CBTN_SZ, layer);
    create_gamepad_btn(vui, VANILLA_AXIS_R_LEFT, 0, "left.svg", RSTICK_X, STICK_Y + CBTN_SZ, CBTN_SZ, CBTN_SZ, layer);
    create_gamepad_btn(vui, VANILLA_BTN_R3, 0, 0, RSTICK_X + CBTN_SZ, STICK_Y + CBTN_SZ, CBTN_SZ, CBTN_SZ, layer);
    create_gamepad_btn(vui, VANILLA_AXIS_R_RIGHT, 0, "right.svg", RSTICK_X + CBTN_SZ + CBTN_SZ, STICK_Y + CBTN_SZ, CBTN_SZ, CBTN_SZ, layer);
    create_gamepad_btn(vui, VANILLA_AXIS_R_DOWN, 0, "down.svg", RSTICK_X + CBTN_SZ, STICK_Y + CBTN_SZ + CBTN_SZ, CBTN_SZ, CBTN_SZ, layer);

    // Face buttons
    create_gamepad_btn(vui, VANILLA_BTN_X, "X", 0, RSTICK_X + CBTN_SZ, DPAD_Y + 0, CBTN_SZ, CBTN_SZ, layer);
    create_gamepad_btn(vui, VANILLA_BTN_Y, "Y", 0, RSTICK_X + 0, DPAD_Y + CBTN_SZ, CBTN_SZ, CBTN_SZ, layer);
    create_gamepad_btn(vui, VANILLA_BTN_A, "A", 0, RSTICK_X + CBTN_SZ + CBTN_SZ, DPAD_Y + CBTN_SZ, CBTN_SZ, CBTN_SZ, layer);
    create_gamepad_btn(vui, VANILLA_BTN_B, "B", 0, RSTICK_X + CBTN_SZ, DPAD_Y + CBTN_SZ + CBTN_SZ, CBTN_SZ, CBTN_SZ, layer);

    // Home
    const int HOME_X = scrw/2-CBTN_SZ/2;
    create_gamepad_btn(vui, VANILLA_BTN_HOME, 0, "home.svg", HOME_X, DPAD_Y + CBTN_SZ + CBTN_SZ/2, CBTN_SZ, CBTN_SZ, layer);

    // TV
    const int ACC_BTN_X = HOME_X;
    const int ACC_BTN_W = (RSTICK_X - ACC_BTN_X);
    const int TV_X = ACC_BTN_X + ACC_BTN_W / 4;
    create_gamepad_btn(vui, VANILLA_BTN_TV, "TV", 0, TV_X, DPAD_Y + CBTN_SZ + CBTN_SZ/2, CBTN_SZ, CBTN_SZ, layer);

    // Power
    const int POWER_X = ACC_BTN_X + ACC_BTN_W * 2 / 4;
    create_gamepad_btn(vui, VANILLA_BTN_POWER, 0, "power.svg", POWER_X, DPAD_Y + CBTN_SZ + CBTN_SZ/2, CBTN_SZ, CBTN_SZ, layer);

    // Plus/minus
    const int PLUSMINUS_X = ACC_BTN_X + ACC_BTN_W * 3 / 4;
    create_gamepad_btn(vui, VANILLA_BTN_PLUS, "+", 0, PLUSMINUS_X, DPAD_Y + CBTN_SZ, CBTN_SZ, CBTN_SZ, layer);
    create_gamepad_btn(vui, VANILLA_BTN_MINUS, "-", 0, PLUSMINUS_X, DPAD_Y + CBTN_SZ + CBTN_SZ, CBTN_SZ, CBTN_SZ, layer);

    // L/ZL
    const int TRIGGER_W = CBTN_SZ * 3;
    const int ZL_X = scrw/3-TRIGGER_W/2;
    create_gamepad_btn(vui, VANILLA_BTN_L, "L", 0, ZL_X, 0, TRIGGER_W, CBTN_SZ, layer);
    create_gamepad_btn(vui, VANILLA_BTN_ZL, "ZL", 0, ZL_X, CBTN_SZ, TRIGGER_W, CBTN_SZ, layer);

    // R/ZR
    const int ZR_X = scrw*2/3-TRIGGER_W/2;
    create_gamepad_btn(vui, VANILLA_BTN_R, "R", 0, ZR_X, 0, TRIGGER_W, CBTN_SZ, layer);
    create_gamepad_btn(vui, VANILLA_BTN_ZR, "ZR", 0, ZR_X, CBTN_SZ, TRIGGER_W, CBTN_SZ, layer);

    // More button
    vui_button_create(vui, scrw-BTN_SZ, 0, BTN_SZ, BTN_SZ, lang(VPI_LANG_MORE), 0, VUI_BUTTON_STYLE_CORNER, layer, return_to_settings, (void *) (intptr_t) layer);

    vui_transition_fade_layer_in(vui, layer, 0, 0);
}