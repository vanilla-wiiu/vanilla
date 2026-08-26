#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <vanilla.h>

#include "config.h"
#include "menu/menu.h"
#include "platform.h"
#include "ui/ui.h"
#include "ui/ui_sdl.h"

#define SCREEN_WIDTH 854
#define SCREEN_HEIGHT 480

#if !defined(ANDROID) && !defined(_WIN32)
#define SDL_main main
#endif

#include "pipemgmt.h"

void display_cli_help(const char **argv);

int SDL_main(int argc, const char **argv)
{
    int ret = 1;

    // Determine whether we're overriding the fullscreen setting in the config
    int override_fs = -1;
    int force_swdec = 0;
    long autoconnect = -1;

    for (int i = 1, consumed; i < argc; i += consumed) {
        consumed = -1;
        if (!strcmp(argv[i], "-w") || !strcmp(argv[i], "--window")) {
            override_fs = 0;
            consumed = 1;
        } else if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--fullscreen")) {
            override_fs = 1;
            consumed = 1;
        } else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--swdec")) {
            force_swdec = 1;
            consumed = 1;
        } else if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "--autoconnect")) {
            if (i + 1 >= argc) {
                vpilog("%s requires an argument\n", argv[i]);
                return 1;
            }

            const char *s_acId = argv[i + 1];
            char *endptr;
            autoconnect = strtol(s_acId, &endptr, 10);
            if (errno == ERANGE || endptr == s_acId || *endptr != '\0' || autoconnect < 0) {
                vpilog("\"%s\" was not a valid argument for %s\n", s_acId, argv[i]);
                return 1;
            }

            consumed = 2;
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            display_cli_help(argv);
            return 0;
        }
        if (consumed <= 0) {
            vpilog("Invalid argument(s): %s\n\n", argv[i]);
            display_cli_help(argv);
            return 1;
        }
    }

    vanilla_install_logger(vpilog_va);

    // Load config
    vpi_config_init();
    if (autoconnect >= vpi_config.connected_console_count) {
        vpilog("Console %li does not exist for auto-connection\n", autoconnect);
        goto exit_config;
    }

    vpi_config.autoconnect = autoconnect;
    vpi_config.force_software_decode |= force_swdec;

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

exit_config:
    vpi_config_free();

    return ret;
}

void display_cli_help(const char **argv)
{
    vpilog("Usage: %s [options]\n\n", argv[0]);
    vpilog("Options:\n");
    vpilog("    -w, --window        Run Vanilla in a window (overrides config)\n");
    vpilog("    -f, --fullscreen    Run Vanilla full screen (overrides config)\n");
    vpilog("    -s, --swdec         Force software decoding (overrides config)\n");
    vpilog("    -a <id>,\n");
    vpilog("    --autoconnect <id>  Auto-connect to a console on startup\n");
    vpilog("                        (<id> is the index of the console in the\n");
    vpilog("                        menu, e.g. 0 is the first console, 1 is\n");
    vpilog("                        the second, etc.)\n");
    vpilog("    -h, --help          Show this help message\n");
}
