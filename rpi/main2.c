#include <stdio.h>

#include "menu/menu.h"
#include "ui/ui.h"
#include "ui/ui_sdl2.h"

#define SCREEN_WIDTH    854
#define SCREEN_HEIGHT   480

int main()
{
    // Initialize UI system
    vui_context_t *vui = vui_alloc(SCREEN_WIDTH, SCREEN_HEIGHT);

    // Initialize SDL2
    int ret = 1;
    if (vui_init_sdl2(vui)) {
        fprintf(stderr, "Failed to initialize VUI\n");
        goto exit;
    }

    vanilla_menu_init(vui);

    while (vui_update_sdl2(vui)) {
    }

    ret = 0;

exit:
    vui_close_sdl2(vui);

    vui_free(vui);

    return ret;
}