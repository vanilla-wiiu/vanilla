#include "menu_gamepad.h"

#include "config.h"
#include "lang.h"
#include "menu/menu.h"
#include "menu/menu_common.h"
#include "menu/menu_settings.h"
#include "ui/ui_anim.h"
#include "ui/ui_keyboard.h"
#include "lib/vanilla.h"

#define MAIN_MENU_ENTRIES 3

static const uint32_t gICON_COLOUR = 0x222222;

static int map_buttons[32];
static size_t map_button_count = 0;

static struct {
    int button;
    int ui_button;
} active_bind_button;

static void reset_active_bind_button()
{
    active_bind_button.button = -1;
    active_bind_button.ui_button = -1;
}

static void reset_map_button_list()
{
    map_button_count = 0;
}

static void return_to_settings(vui_context_t *vui, int btn, void *v)
{
    vui_clear_key_listener(vui);
    int layer = (intptr_t) v;
    vui_transition_fade_layer_out(vui, layer, vpi_menu_settings, 0);
}

void vpi_menu_toggle_swap_abxy(vui_context_t *vui, int btn, void *v)
{
    vpi_config.swap_abxy = !vpi_config.swap_abxy;
    vpi_config_save();

    vui_button_update_checked(vui, btn, vpi_config.swap_abxy);
}

static int find_current_keybind(vui_context_t *ctx, int btn)
{
    return vui_get_key_mapping(ctx, btn);
}

static void return_to_gamepad(vui_context_t *vui, int btn, void *v)
{
    vui_clear_key_listener(vui);
    int layer = (intptr_t) v;
    vui_transition_fade_layer_out(vui, layer, vpi_menu_gamepad, 0);
}

static void transition_to_keybinds(vui_context_t *vui, int btn, void *v)
{
    vui_clear_key_listener(vui);
    int layer = (intptr_t) v;
    vui_transition_fade_layer_out(vui, layer, vpi_menu_key_bindings, 0);
}

static void tranisiton_to_key_bindings_more(vui_context_t *vui, int btn, void *v)
{
    vui_clear_key_listener(vui);
    int layer = (intptr_t) v;
    vui_transition_fade_layer_out(vui, layer, vpi_menu_key_bindings_more, 0);
}

static void update_button_icons(vui_context_t *ctx)
{
    for (size_t i = 0; i < map_button_count; i++) {
        int btn_id = map_buttons[i];
        int vanilla_btn = (intptr_t) vui_button_get_click_handler_data(ctx, btn_id);
        vui_button_update_icon(ctx, btn_id, vui_get_keyicon_from_scancode(find_current_keybind(ctx, vanilla_btn)));
    }
}

static void animate_button(vui_context_t *ctx, int64_t time, void *userdata)
{
    if (active_bind_button.ui_button != -1) {
        vui_button_update_icon_mod(ctx, active_bind_button.ui_button, ((time / 500000) % 2 == 1) ? gICON_COLOUR : 0xFFFF0000);
    }
}

static int set_default_button_icon_mod(vui_context_t *vui, int button)
{
    vui_button_update_icon_mod(vui, button, gICON_COLOUR);
}

static void cancel_key_listen(vui_context_t *ctx, void *userdata)
{
    vui_clear_key_listener(ctx);
    set_default_button_icon_mod(ctx, active_bind_button.ui_button);
    reset_active_bind_button();
    update_button_icons(ctx);
}

static void listen_key_binding(vui_context_t *ctx, int key, void *userdata)
{
    vui_set_key_mapping(ctx, active_bind_button.button, key);
    cancel_key_listen(ctx, userdata);
}

static void vpi_bind_callback(vui_context_t *ctx, int button, void *userdata)
{
    active_bind_button.button = (int) (intptr_t)userdata;
    active_bind_button.ui_button = button;
    vui_set_key_listener(ctx, listen_key_binding, cancel_key_listen, userdata);
}

static void vpi_default_keyboard_bindings(vui_context_t *ctx, int button, void *userdata)
{
    vpi_config_reset_default_controls();
    cancel_key_listen(ctx, NULL);
    update_button_icons(ctx);
}

static int create_key_bind_button(vui_context_t *vui, int vanilla_btn, int x, int y, int w, int h, int layer)
{
    const char *button_icon = vui_get_keyicon_from_scancode(find_current_keybind(vui, vanilla_btn));
    int button = vui_button_create(vui, x, y, w, h, NULL, button_icon, VUI_BUTTON_STYLE_BUTTON, layer, vpi_bind_callback, (void *) (intptr_t) vanilla_btn);
    vui_button_update_font_size(vui, button, VUI_FONT_SIZE_SMALL);
    set_default_button_icon_mod(vui, button);
    map_buttons[map_button_count] = button;
    map_button_count++;
    return button;
}

void vpi_menu_key_bindings_more(vui_context_t *vui, void *v)
{
    vui_reset(vui);
    reset_active_bind_button();
    reset_map_button_list();

    int layer = vui_layer_create(vui);

    int SCREEN_WIDTH, SCREEN_HEIGHT;
    vui_get_screen_size(vui, &SCREEN_WIDTH, &SCREEN_HEIGHT);

    const int btn_w = 50;
    const int btn_h = 50;
    const int padding = 15;

    const int img_w = btn_w;
    const int img_h = btn_h;
    int img_x = SCREEN_WIDTH / 6 - img_w;
    const int img_y = SCREEN_HEIGHT / 4 - img_h / 2;
    int btn_offset = 0;

    {
        btn_offset = 0;
        img_x += img_w * 5;

        // Special Action Buttons
        create_key_bind_button(vui, VPI_ACTION_TOGGLE_RECORDING, img_x + img_w + padding, img_y + padding + (img_w * btn_offset), btn_w, btn_h, layer);
        vui_image_create(vui, img_x, img_y + padding + (img_h * btn_offset++), img_w, img_h, "recording_icon.svg", layer);
        create_key_bind_button(vui, VPI_ACTION_TOGGLE_FULLSCREEN, img_x + img_w + padding, img_y + padding + (img_w * btn_offset), btn_w, btn_h, layer);
        vui_image_create(vui, img_x, img_y + padding + (img_h * btn_offset++), img_w, img_h, "fullscreen_icon.svg", layer);
        create_key_bind_button(vui, VPI_ACTION_SCREENSHOT, img_x + img_w + padding, img_y + padding + (img_w * btn_offset), btn_w, btn_h, layer);
        vui_image_create(vui, img_x, img_y + padding + (img_h * btn_offset++), img_w, img_h, "screenshot_icon.svg", layer);
    }

    vui_button_create(vui, 0, SCREEN_HEIGHT - BTN_SZ, BTN_SZ, BTN_SZ, lang(VPI_LANG_RESET), 0, VUI_BUTTON_STYLE_CORNER, layer, vpi_default_keyboard_bindings, NULL);

    vpi_menu_create_back_button(vui, layer, transition_to_keybinds, (void *) (intptr_t) layer);

    vui_transition_fade_layer_in(vui, layer, 0, 0);

    vui_start_passive_animation(vui, animate_button, 0);
}

void vpi_menu_key_bindings(vui_context_t *vui, void *v)
{
    vui_reset(vui);
    reset_active_bind_button();
    reset_map_button_list();

    int layer = vui_layer_create(vui);

    int SCREEN_WIDTH, SCREEN_HEIGHT;
    vui_get_screen_size(vui, &SCREEN_WIDTH, &SCREEN_HEIGHT);

    const int img_w = SCREEN_WIDTH / 2;
    const int img_h = SCREEN_HEIGHT / 2;
    const int img_x = SCREEN_WIDTH / 2 - img_w / 2;
    const int img_y = SCREEN_HEIGHT / 2 - img_h / 2;
    vui_image_create(vui, img_x, img_y, img_w, img_h, "gamepad.svg", layer);

    const int btn_width = 50;
    const int btn_height = 50;

    {
        // SHOULDER BUTTONS
        create_key_bind_button(vui, VANILLA_BTN_ZL, img_x, img_y - btn_height * 2, btn_width, btn_height, layer);
        create_key_bind_button(vui, VANILLA_BTN_L, img_x, img_y - btn_height, btn_width, btn_height, layer);
        create_key_bind_button(vui, VANILLA_BTN_ZR, img_x + img_w - btn_width, img_y - btn_height * 2, btn_width, btn_height, layer);
        create_key_bind_button(vui, VANILLA_BTN_R, img_x + img_w - btn_width, img_y - btn_height, btn_width, btn_height, layer);

        const int dpad_x_ref = img_w / 4 - btn_width / 2;
        const int dpad_y_ref = img_y + img_h * 8 / 8;

        const int l_stick_y_ref = img_y + img_h * 2 / 8;

        const int abxy_x_ref = img_x + img_w + img_w / 4 - btn_width / 2;
        const int abxy_y_ref = dpad_y_ref;

        // LSTICK
        create_key_bind_button(vui, VANILLA_AXIS_L_UP, dpad_x_ref, l_stick_y_ref - btn_height * 1.5, btn_width, btn_height, layer);
        create_key_bind_button(vui, VANILLA_AXIS_L_DOWN, dpad_x_ref, l_stick_y_ref + btn_height / 2, btn_width, btn_height, layer);
        create_key_bind_button(vui, VANILLA_AXIS_L_LEFT, dpad_x_ref - btn_width, l_stick_y_ref - btn_height / 2, btn_width, btn_height, layer);
        create_key_bind_button(vui, VANILLA_AXIS_L_RIGHT, dpad_x_ref + btn_width, l_stick_y_ref - btn_height / 2, btn_width, btn_height, layer);
        create_key_bind_button(vui, VANILLA_BTN_L3, dpad_x_ref, l_stick_y_ref - btn_height / 2, btn_width, btn_height, layer);

        // DPAD
        create_key_bind_button(vui, VANILLA_BTN_UP, dpad_x_ref, dpad_y_ref - btn_height * 1.5, btn_width, btn_height, layer);
        create_key_bind_button(vui, VANILLA_BTN_DOWN, dpad_x_ref, dpad_y_ref + btn_height / 2, btn_width, btn_height, layer);
        create_key_bind_button(vui, VANILLA_BTN_LEFT, dpad_x_ref - btn_width, dpad_y_ref - btn_height / 2, btn_width, btn_height, layer);
        create_key_bind_button(vui, VANILLA_BTN_RIGHT, dpad_x_ref + btn_width, dpad_y_ref - btn_height / 2, btn_width, btn_height, layer);

        // RSTICK
        create_key_bind_button(vui, VANILLA_AXIS_R_UP, abxy_x_ref, l_stick_y_ref - btn_height * 1.5, btn_width, btn_height, layer);
        create_key_bind_button(vui, VANILLA_AXIS_R_DOWN, abxy_x_ref, l_stick_y_ref + btn_height / 2, btn_width, btn_height, layer);
        create_key_bind_button(vui, VANILLA_AXIS_R_DOWN, abxy_x_ref - btn_width, l_stick_y_ref - btn_height / 2, btn_width, btn_height, layer);
        create_key_bind_button(vui, VANILLA_AXIS_R_RIGHT, abxy_x_ref + btn_width, l_stick_y_ref - btn_height / 2, btn_width, btn_height, layer);
        create_key_bind_button(vui, VANILLA_BTN_R3, abxy_x_ref, l_stick_y_ref - btn_height / 2, btn_width, btn_height, layer);

        // ABXY
        create_key_bind_button(vui, VANILLA_BTN_UP, abxy_x_ref, abxy_y_ref - btn_height * 1.5, btn_width, btn_height, layer);
        create_key_bind_button(vui, VANILLA_BTN_DOWN, abxy_x_ref, abxy_y_ref + btn_height / 2, btn_width, btn_height, layer);
        create_key_bind_button(vui, VANILLA_BTN_LEFT, abxy_x_ref - btn_width, abxy_y_ref - btn_height / 2, btn_width, btn_height, layer);
        create_key_bind_button(vui, VANILLA_BTN_RIGHT, abxy_x_ref + btn_width, abxy_y_ref - btn_height / 2, btn_width, btn_height, layer);

        const int plus_minus_x_ref = img_w + btn_width * 3.5;

        const int home_tv_y_ref = img_y + img_h;

        const int plus_minus_y_ref = img_y + img_h - btn_height / 4;

        create_key_bind_button(vui, VANILLA_BTN_PLUS, plus_minus_x_ref, plus_minus_y_ref, btn_width, btn_height, layer);
        create_key_bind_button(vui, VANILLA_BTN_MINUS, plus_minus_x_ref, plus_minus_y_ref + btn_height, btn_width, btn_height, layer);

        create_key_bind_button(vui, VANILLA_BTN_HOME, img_w - btn_width / 2, home_tv_y_ref, btn_width, btn_height, layer);
        create_key_bind_button(vui, VANILLA_BTN_TV, img_w + btn_width * 1.5, home_tv_y_ref, btn_width, btn_height, layer);
    }

    vui_button_create(vui, 0, SCREEN_HEIGHT - BTN_SZ, BTN_SZ, BTN_SZ, lang(VPI_LANG_RESET), 0, VUI_BUTTON_STYLE_CORNER, layer, vpi_default_keyboard_bindings, NULL);

    vui_button_create(vui, SCREEN_WIDTH - BTN_SZ, 0, BTN_SZ, BTN_SZ, lang(VPI_LANG_MORE), 0, VUI_BUTTON_STYLE_CORNER, layer, tranisiton_to_key_bindings_more, (void *) (intptr_t) layer);
    vpi_menu_create_back_button(vui, layer, return_to_gamepad, (void *) (intptr_t) layer);

    update_button_icons(vui);

    vui_transition_fade_layer_in(vui, layer, 0, 0);

    vui_start_passive_animation(vui, animate_button, 0);
}

void vpi_menu_gamepad(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    int layer = vui_layer_create(vui);

    int SCREEN_WIDTH, SCREEN_HEIGHT;
    vui_get_screen_size(vui, &SCREEN_WIDTH, &SCREEN_HEIGHT);

    int list_item_width = SCREEN_WIDTH - BTN_SZ - BTN_SZ;
    int list_item_height = (SCREEN_HEIGHT - BTN_SZ - BTN_SZ) / MAIN_MENU_ENTRIES;

    int swap_abxy_button = vui_button_create(vui, BTN_SZ, BTN_SZ + list_item_height * 0, list_item_width, list_item_height, lang(VPI_LANG_SWAP_ABXY_BUTTONS), NULL, VUI_BUTTON_STYLE_LIST, layer, vpi_menu_toggle_swap_abxy, 0);
    vui_button_update_checkable(vui, swap_abxy_button, 1);
    vui_button_update_checked(vui, swap_abxy_button, vpi_config.swap_abxy);

    int keyboard_bind_button = vui_button_create(vui, BTN_SZ, BTN_SZ + list_item_height * 1, list_item_width, list_item_height, lang(VPI_LANG_KEYBOARD_CONTROLS), NULL, VUI_BUTTON_STYLE_LIST, layer, transition_to_keybinds, (void *)(intptr_t)layer);

    // Back button
    vpi_menu_create_back_button(vui, layer, return_to_settings, (void *) (intptr_t) layer);

    vui_transition_fade_layer_in(vui, layer, 0, 0);
}
