#include "menu.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <vanilla.h>

#include "config.h"
#include "menu_common.h"
#include "menu_game.h"
#include "menu_main.h"
#include "platform.h"

void vpi_mic_callback(void *userdata, const uint8_t *data, size_t len)
{
	vanilla_send_audio(data, len);
}

void vpi_menu_init(vui_context_t *vui)
{
	// Set microphone callback
	vui_mic_callback_set(vui, vpi_mic_callback, 0);

    // Start with main menu
    vpi_menu_main(vui, 0);
}

void get_valid_filename(const char *fmt, char *abs_buf, size_t size_abs_buf, const char *preferred_dir)
{
    char buf[100];
    int index = 0;
    do {
        index++;
        snprintf(buf, sizeof(buf), fmt, index);
        vpi_get_data_filename(abs_buf, size_abs_buf, buf, preferred_dir);
    } while (access(abs_buf, F_OK) == 0);
}

void vpi_menu_action(vui_context_t *vui, vpi_extra_action_t action)
{
    switch (action) {
    case VPI_ACTION_SCREENSHOT:
    {
        char ss_fn[4096];
        get_valid_filename("Screenshot-%04i.png", ss_fn, sizeof(ss_fn), vpi_config.recording_dir);
        vpi_decode_screenshot(ss_fn);
        break;
    }
    case VPI_ACTION_TOGGLE_RECORDING:
    {
        int recording = vpi_decode_is_recording();
        if (!recording) {
            char mov_fn[4096];
            get_valid_filename("Recording-%04i.mp4", mov_fn, sizeof(mov_fn), vpi_config.recording_dir);
            vpi_decode_record(mov_fn);
        } else {
            vpi_decode_record_stop();
        }
        break;
    }
    case VPI_ACTION_DISCONNECT:
    {
        if (vui_game_mode_get(vui)) {
            // Send shutdown signal
            vpi_game_shutdown();
        } else {
            // Quit Vanilla entirely
            vpi_menu_quit_vanilla(vui);
        }
        break;
    }
    }
}
