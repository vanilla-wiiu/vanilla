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

#if defined(__linux__) && !defined(ANDROID) && !defined(__ANDROID__)
#include <pthread.h>
#include <string.h>

// Calling system() is synchronous and causes a noticeable pause during
// gameplay. This allows us to thunk from a separate detached thread so it
// doesn't interrupt the main thread.
static void *run_volume_helper(void *command)
{
    (void) system((const char *) command);
    return NULL;
}
#endif

void vpi_mic_callback(void *userdata, const uint8_t *data, size_t len)
{
	vanilla_send_audio(data, len);
}

void vpi_menu_init(vui_context_t *vui)
{
	// Set microphone callback
	vui_mic_callback_set(vui, vpi_mic_callback, 0);

    if (vpi_config.autoconnect != -1) {
        vpi_menu_game(vui, (void *)(intptr_t) vpi_config.autoconnect);
    } else {
        // Start with main menu
        vpi_menu_main(vui, 0);
    }
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
    case VPI_ACTION_TOGGLE_FULLSCREEN:
    {
        vpi_config.fullscreen = !vpi_config.fullscreen;
        vpi_config_save();
        vui_set_fullscreen(vui, vpi_config.fullscreen);
        break;
    }
    case VPI_ACTION_VOLUME_UP:
    case VPI_ACTION_VOLUME_DOWN:
    {
#if defined(__linux__) && !defined(ANDROID) && !defined(__ANDROID__)
        // On some platforms (specifically our Buildroot platforms like the
        // Raspberry Pi and Nintendo Switch), there's no built-in handler for
        // the volume keys. So here we handle them ourselves.
        const char *helper = "/usr/libexec/vanilla-volume";
        if (access(helper, X_OK) == 0) {
            const char *command = action == VPI_ACTION_VOLUME_UP
                ? "/usr/libexec/vanilla-volume up"
                : "/usr/libexec/vanilla-volume down";
            pthread_t thread;
            int error = pthread_create(&thread, NULL, run_volume_helper, (void *) command);
            if (error == 0) {
                error = pthread_detach(thread);
            }
            if (error != 0) {
                vpilog("Failed to start volume helper thread: %s\n", strerror(error));
            }
        }
#endif
        break;
    }
    }
}
