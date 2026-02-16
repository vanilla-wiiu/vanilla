#include "menu_settings.h"

#include <vanilla.h>
#include <unistd.h>

#include "config.h"
#include "lang.h"
#include "menu_common.h"
#include "menu_connection.h"
#include "menu_gamepad.h"
#include "menu_main.h"
#include "menu_region.h"
#include "menu_sudo_warning.h"
#include "pipe/def.h"
#include "ui/ui_anim.h"

vui_context_t *vui_count[16];
vpi_lang_t text_count[16];
int old_back_layer;

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

static void change_language(vui_context_t *vui, int button, void *v)
{
    switch_lang(get_lang());
    for(int i = 0; i < 16; i++){
        if(!vui_count[i]) {continue;}
        vui_button_update_text(vui_count[i], i, lang(text_count[i]));
        vpi_menu_create_back_button(vui, old_back_layer, return_to_main, (void *) (intptr_t) old_back_layer);
    }
}


#ifdef VANILLA_POLKIT_AVAILABLE
static void do_polkit_install(vui_context_t *vui, void *v)
{
	int install = (intptr_t) v;
	if (install) {
		vanilla_install_polkit(vpi_config.server_address);
	} else {
		vanilla_uninstall_polkit(vpi_config.server_address);
	}
	vpi_menu_settings(vui, 0);
}

static void thunk_to_start_pipe_for_polkit_rule_install(vui_context_t *vui, void *v)
{
	vpi_menu_start_pipe(vui, 0, do_polkit_install, v, 0, 0);
}

int tmp_fglayer;
static void install_polkit_rule_ack(vui_context_t *vui, int button, void *v)
{
	vui_transition_fade_layer_out(vui, tmp_fglayer, thunk_to_start_pipe_for_polkit_rule_install, v);
}

static void install_polkit_rule_cancel(vui_context_t *vui, int button, void *v)
{
	vui_transition_fade_layer_out(vui, tmp_fglayer, vpi_menu_settings, 0);
}

static void install_polkit_rule(vui_context_t *vui, void *v)
{
    vui_reset(vui);

	tmp_fglayer = vui_layer_create(vui);

	vpi_menu_create_sudo_warning(vui, tmp_fglayer, install_polkit_rule_ack, v, install_polkit_rule_cancel, 0);

	vui_transition_fade_layer_in(vui, tmp_fglayer, 0, 0);
}

static void transition_to_install_polkit_rule(vui_context_t *vui, int button, void *v)
{
    int layer = (intptr_t) v;
    vui_transition_fade_layer_out(vui, layer, install_polkit_rule, (void *) (intptr_t) 1);
}

static void transition_to_uninstall_polkit_rule(vui_context_t *vui, int button, void *v)
{
    int layer = (intptr_t) v;
    vui_transition_fade_layer_out(vui, layer, thunk_to_start_pipe_for_polkit_rule_install, 0);
}
#endif

static void toggle_fullscreen(vui_context_t *vui, int button, void *v)
{
    vpi_config.fullscreen = !vpi_config.fullscreen;
    vpi_config_save();
    vui_set_fullscreen(vui, vpi_config.fullscreen);
    vui_button_update_checked(vui, button, vpi_config.fullscreen);
}

void vpi_menu_settings(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    int fglayer = vui_layer_create(vui);
    old_back_layer = fglayer;

    int scrw, scrh;
    vui_get_screen_size(vui, &scrw, &scrh);

    const int FS_SETTING = 3;
	const int PW_SKIP_SETTING = 4;

    static int SETTINGS_NAMES[] = {
        VPI_LANG_CONNECTION,
        VPI_LANG_CONTROLS,
        VPI_LANG_REGION,
        -1,
        -1,
        VPI_LANG_LANG_CHOICE,
    };

    // Highlight currently implemented functionality
    static vui_button_callback_t SETTINGS_ACTION[] = {
        transition_to_connection,
        transition_to_gamepad,
        transition_to_region,
        0,
        0,
        change_language,
    };


    static const int button_count = sizeof(SETTINGS_NAMES) / sizeof(int);
    int buttons[button_count];

#ifdef VANILLA_GUI_ENABLE_WINDOWED
    SETTINGS_NAMES[FS_SETTING] = VPI_LANG_FULLSCREEN;
	SETTINGS_ACTION[FS_SETTING] = toggle_fullscreen;
#endif

#ifdef VANILLA_POLKIT_AVAILABLE
	int pw_skip_str;
	vui_button_callback_t pw_skip_action;
	if (access(POLKIT_ACTION_DST, F_OK) == 0) {
        SETTINGS_NAMES[PW_SKIP_SETTING] = VPI_LANG_DISABLE_PASSWORD_SKIP;
		SETTINGS_ACTION[PW_SKIP_SETTING] = transition_to_uninstall_polkit_rule;
    } else {
		SETTINGS_NAMES[PW_SKIP_SETTING] = VPI_LANG_ENABLE_PASSWORD_SKIP;
		SETTINGS_ACTION[PW_SKIP_SETTING] = transition_to_install_polkit_rule;
	}
#endif

	int btnw = scrw*3/4;
	int btnx = scrw/2 - btnw/2;
	int btny = scrh/16;
    int btnh = BTN_SZ/1.1;
	for (int index = 0; index < button_count; index++) {
		if (SETTINGS_ACTION[index]) {
            vui_count[index] = vui;
            text_count[index] = SETTINGS_NAMES[index];
			buttons[index] = vui_button_create(vui, btnx, btny, btnw, btnh, lang(SETTINGS_NAMES[index]), 0, VUI_BUTTON_STYLE_BUTTON, fglayer, SETTINGS_ACTION[index], (void *) (intptr_t) fglayer);
            btny += btnh;
		}
	}

#ifdef VANILLA_GUI_ENABLE_WINDOWED
    // Make full screen button checkable
    vui_button_update_checkable(vui, buttons[FS_SETTING], 1);
    vui_button_update_checked(vui, buttons[FS_SETTING], vpi_config.fullscreen);
#endif

    // Back button
    
    vpi_menu_create_back_button(vui, fglayer, return_to_main, (void *) (intptr_t) fglayer);

    vui_transition_fade_layer_in(vui, fglayer, 0, 0);
}
