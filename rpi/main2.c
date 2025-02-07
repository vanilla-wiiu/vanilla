#include <stdio.h>
#include <string.h>

#include "menu/menu.h"
#include "ui/ui.h"
#include "ui/ui_sdl.h"

#define SCREEN_WIDTH    854
#define SCREEN_HEIGHT   480

int main(int argc, const char **argv)
{
    int fs = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-f")) {
            fs = 1;
        }
    }

    // Initialize UI system
    vui_context_t *vui = vui_alloc(SCREEN_WIDTH, SCREEN_HEIGHT);

    // Initialize SDL2
    int ret = 1;
    if (vui_init_sdl(vui, fs)) {
        fprintf(stderr, "Failed to initialize VUI\n");
        goto exit;
    }

    vanilla_menu_init(vui);

    while (vui_update_sdl(vui)) {
    }

    ret = 0;

exit:
    vui_close_sdl(vui);

    vui_free(vui);

    return ret;
}