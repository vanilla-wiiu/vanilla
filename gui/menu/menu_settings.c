#include "menu_settings.h"

#include "lang.h"
#include "menu_common.h"
#include "menu_connection.h"
#include "menu_gamepad.h"
#include "menu_main.h"
#include "menu_region.h"
#include "ui/ui_anim.h"

static void return_to_main(vui_context_t *vui, int btn, void *v)
{
    int layer = (intptr_t) v;
    vui_transition_fade_layer_out(vui, layer, vpi_menu_main, (void *) (intptr_t) 1);
}

static void transition_to_gamepad(vui_context_t *vui, int button, void *v)
{
    int layer = (intptr_t) v;
    vui_transition_fade_layer_out(vui, layer, vpi_menu_gamepad, 0);
}

static void transition_to_connection(vui_context_t *vui, int button, void *v)
{
    int layer = (intptr_t) v;
    vui_transition_fade_layer_out(vui, layer, vpi_menu_connection, 0);
}

static void transition_to_region(vui_context_t *vui, int button, void *v)
{
    int layer = (intptr_t) v;
    vui_transition_fade_layer_out(vui, layer, vpi_menu_region, 0);
}

static void thunk_to_quit(vui_context_t *vui, int button, void *v)
{
    vpi_menu_quit_vanilla(vui);
}

void vpi_menu_settings(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    int fglayer = vui_layer_create(vui);

    uint8_t console = (uintptr_t) v;

    int scrw, scrh;
    vui_get_screen_size(vui, &scrw, &scrh);

    static const int SETTINGS_NAMES[] = {
        VPI_LANG_CONNECTION,
        VPI_LANG_GAMEPAD,
        VPI_LANG_REGION,
        VPI_LANG_QUIT,
    };

    // Highlight currently implemented functionality
    static const int SETTINGS_ENABLED[] = {
        1,
        1,
        1,
        1,
    };

    // Highlight currently implemented functionality
    static const vui_button_callback_t SETTINGS_ACTION[] = {
        transition_to_connection,
        transition_to_gamepad,
        transition_to_region,
        thunk_to_quit,
    };

    const int settings_cols = 2;
    const int settings_rows = 2;
    const int settings_w = scrw/3;
    const int settings_h = scrh/3;
    const int settings_x = scrw/2 - (settings_w*settings_cols)/2;
    const int settings_y = scrh/2 - (settings_h*settings_rows)/2;
    for (int y = 0; y < settings_rows; y++) {
        for (int x = 0; x < settings_cols; x++) {
            int index = y * settings_cols + x;
            int b = vui_button_create(vui, settings_x + settings_w * x, settings_y + settings_h * y, settings_w, settings_h, lang(SETTINGS_NAMES[index]), 0, VUI_BUTTON_STYLE_BUTTON, fglayer, SETTINGS_ACTION[index], (void *) (intptr_t) fglayer);
            vui_button_update_enabled(vui, b, SETTINGS_ENABLED[index]);
        }
    }

    // Back button
    vpi_menu_create_back_button(vui, fglayer, return_to_main, (void *) (intptr_t) fglayer);

    vui_transition_fade_layer_in(vui, fglayer, 0, 0);
}