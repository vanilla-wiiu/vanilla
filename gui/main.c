#include <stdio.h>
#include <string.h>
#include <vanilla.h>

#include "config.h"
#include "menu/menu.h"
#include "platform.h"
#include "ui/ui.h"
#include "ui/ui_sdl.h"
#include "lang.h"
char curr_lang[3] = "en";

#define SCREEN_WIDTH    854
#define SCREEN_HEIGHT   480

#if !defined(ANDROID) && !defined(_WIN32)
#define SDL_main main
#endif

#include "pipemgmt.h"

void display_cli_help(const char **argv);

int SDL_main(int argc, const char **argv)
{
    // Default to full screen unless "-w" is specified
    int override_fs = -1;
	for (int i = 1, consumed; i < argc; i += consumed) {
		consumed = -1;
		 if (!strcmp(argv[i], "-w") || !strcmp(argv[i], "--window")) {
			override_fs = 0;
			consumed = 1;
		} else if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--fullscreen")) {
            override_fs = 1;
            consumed = 1;
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			display_cli_help(argv);
			return 0;
		} else if (!strcmp(argv[i], "-fr") || !strcmp(argv[i], "--french")){
            consumed = 1;
            strcpy(curr_lang, "fr");
        }
		if (consumed <= 0) {
			vpilog("Invalid argument(s): %s\n\n", argv[i]);
			display_cli_help(argv);
			return 1;
		}
	}
    lang_choice(curr_lang);

    vanilla_install_logger(vpilog_va);

    // Load config
    vpi_config_init();

#ifndef VANILLA_GUI_ENABLE_WINDOWED
    // Window mode is disabled, so we'll just enable fullscreen
    override_fs = 1;
#endif

    // Check if override fullscreen, if so set it in the config, but don't save
    if (override_fs != -1) {
        vpi_config.fullscreen = override_fs;
    }

    // Initialize UI system
    vui_context_t *vui = vui_alloc(SCREEN_WIDTH, SCREEN_HEIGHT);

    // Initialize SDL2
    int ret = 1;
    if (vui_init_sdl(vui, vpi_config.fullscreen)) {
        vpilog("Failed to initialize VUI\n");
        goto exit;
    }

    vpi_menu_init(vui);

    while (vui_update_sdl(vui)) {
        vpi_update_pipe();
    }

    ret = 0;

    vpi_stop_pipe();

exit:
    vui_close_sdl(vui);

    vui_free(vui);

    vpi_config_free();

    return ret;
}

void display_cli_help(const char **argv) {
	vpilog("Usage: %s [options]\n\n", argv[0]);
	vpilog("Options:\n");
	vpilog("    -w, --window        Run Vanilla in a window (overrides config)\n");
	vpilog("    -f, --fullscreen    Run Vanilla full screen (overrides config)\n");
	vpilog("    -h, --help          Show this help message\n");
    vpilog("    -fr, --french       Run Vanilla in the french language\n");
}
