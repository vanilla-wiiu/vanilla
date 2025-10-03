#include "menu_settings.h"

#include <vanilla.h>

#include "config.h"
#include "lang.h"
#include "menu_common.h"
#include "menu_connection.h"
#include "menu_gamepad.h"
#include "menu_main.h"
#include "menu_region.h"
#include "menu_sudo_warning.h"
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

static void do_polkit_install(vui_context_t *vui, void *v)
{
	vanilla_install_polkit(vpi_config.server_address);
	vpi_menu_settings(vui, v);
}

static void thunk_to_start_pipe_for_polkit_rule_install(vui_context_t *vui, void *v)
{
	vpi_menu_start_pipe(vui, 0, do_polkit_install, 0, 0, 0);
}

static void install_polkit_rule_ack(vui_context_t *vui, int button, void *v)
{
    int layer = (intptr_t) v;
	vui_transition_fade_layer_out(vui, layer, thunk_to_start_pipe_for_polkit_rule_install, 0);
}

static void install_polkit_rule_cancel(vui_context_t *vui, int button, void *v)
{
    int layer = (intptr_t) v;
	vui_transition_fade_layer_out(vui, layer, vpi_menu_settings, 0);
}

static void install_polkit_rule(vui_context_t *vui, void *v)
{
    vui_reset(vui);

	int fglayer = vui_layer_create(vui);

	vpi_menu_create_sudo_warning(vui, fglayer, install_polkit_rule_ack, (void *) (intptr_t) fglayer, install_polkit_rule_cancel, (void *) (intptr_t) fglayer);

	vui_transition_fade_layer_in(vui, fglayer, 0, 0);
}

static void transition_to_install_polkit_rule(vui_context_t *vui, int button, void *v)
{
    int layer = (intptr_t) v;
    vui_transition_fade_layer_out(vui, layer, install_polkit_rule, 0);
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
#ifdef VANILLA_PIPE_AVAILABLE
        VPI_LANG_ENABLE_PASSWORD_SKIP,
#endif
    };

    // Highlight currently implemented functionality
    static const vui_button_callback_t SETTINGS_ACTION[] = {
        transition_to_connection,
        transition_to_gamepad,
        transition_to_region,
#ifdef VANILLA_PIPE_AVAILABLE
        transition_to_install_polkit_rule,
#endif
    };

	int btnw = scrw*3/4;
	int btnx = scrw/2 - btnw/2;
	int btny = scrh/10;
	for (int index = 0; index < sizeof(SETTINGS_NAMES)/sizeof(int); index++) {
		vui_button_create(vui, btnx, btny + BTN_SZ * index, btnw, BTN_SZ, lang(SETTINGS_NAMES[index]), 0, VUI_BUTTON_STYLE_BUTTON, fglayer, SETTINGS_ACTION[index], (void *) (intptr_t) fglayer);
	}

    // Back button
    vpi_menu_create_back_button(vui, fglayer, return_to_main, (void *) (intptr_t) fglayer);

    vui_transition_fade_layer_in(vui, fglayer, 0, 0);
}
