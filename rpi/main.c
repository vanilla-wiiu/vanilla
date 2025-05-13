#include <stdio.h>
#include <string.h>
#include <vanilla.h>

#include "config.h"
#include "menu/menu.h"
#include "platform.h"
#include "ui/ui.h"
#include "ui/ui_sdl.h"

#define SCREEN_WIDTH    854
#define SCREEN_HEIGHT   480

#if !defined(ANDROID) && !defined(_WIN32)
#define SDL_main main
#endif

#include "game/game_main.h"
#include "pipemgmt.h"

int SDL_main(int argc, const char **argv)
{
    // Default to full screen unless "-w" is specified
    int fs = 1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-w")) {
            fs = 0;
        }
    }

    vanilla_install_logger(vpilog_va);

    // Load config
    vpi_config_init();

    // Initialize UI system
    vui_context_t *vui = vui_alloc(SCREEN_WIDTH, SCREEN_HEIGHT);

    // Initialize SDL2
    int ret = 1;
    if (vui_init_sdl(vui, fs)) {
        vpilog("Failed to initialize VUI\n");
        goto exit;
    }

    vpi_menu_init(vui);

    while (vui_update_sdl(vui)) {
    }

    ret = 0;

    vpi_stop_pipe();

exit:
    vui_close_sdl(vui);

    vui_free(vui);

    vpi_config_free();

    return ret;
}