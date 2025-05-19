#include "menu.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <vanilla.h>

#include "game/game_decode.h"
#include "game/game_main.h"
#include "menu_common.h"
#include "menu_main.h"
#include "platform.h"

void vpi_menu_init(vui_context_t *vui)
{
    // Start with main menu
    vpi_menu_main(vui, 0);
}

void get_valid_filename(const char *fmt, char *abs_buf, size_t size_abs_buf)
{
    char buf[100];
    int index = 0;
    do {
        index++;
        snprintf(buf, sizeof(buf), fmt, index);
        vpi_get_data_filename(abs_buf, size_abs_buf, buf);
    } while (access(abs_buf, F_OK) == 0);
}

void vpi_menu_action(vui_context_t *vui, vpi_extra_action_t action)
{
    switch (action) {
    case VPI_ACTION_SCREENSHOT:
    {
        char ss_fn[4096];
        get_valid_filename("Screenshot-%04i.png", ss_fn, sizeof(ss_fn));
        vpi_decode_screenshot(ss_fn);
        break;
    }
    case VPI_ACTION_TOGGLE_RECORDING:
    {
        int recording = vpi_decode_is_recording();
        if (!recording) {
            char mov_fn[4096];
            get_valid_filename("Recording-%04i.mp4", mov_fn, sizeof(mov_fn));
            vpi_decode_record(mov_fn);
        } else {
            vpi_decode_record_stop();
        }
        break;
    }
    case VPI_ACTION_DISCONNECT:
    {
        if (vui_game_mode_get(vui)) {
            // Simulate shutdown
            vpi_game_set_error(VANILLA_ERR_SHUTDOWN);
        } else {
            // Quit Vanilla entirely
            vpi_menu_quit_vanilla(vui);
        }
        break;
    }
    }
}
