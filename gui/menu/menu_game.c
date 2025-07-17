#include "menu_game.h"

#include <stdio.h>
#include <vanilla.h>

#include "config.h"
#include "lang.h"
#include "game/game_decode.h"
#include "game/game_main.h"
#include "menu_common.h"
#include "menu_main.h"
#include "ui/ui_anim.h"

static int status_lbl;
static struct {
    int64_t last_power_time;
} menu_game_ctx;

void back_to_main_menu(vui_context_t *vui, int button, void *v)
{
    int layer = (intptr_t) v;
    vui_transition_fade_layer_out(vui, layer, vpi_menu_main, (void *) (intptr_t) 1);
}

void show_error(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    vui_enable_background(vui, 1);

    int scrw, scrh;
    vui_get_screen_size(vui, &scrw, &scrh);

    int layer = vui_layer_create(vui);

    vui_label_create(vui, 0, scrh * 1 / 6, scrw, scrh, lang(VPI_LANG_ERROR), vui_color_create(0.25f,0.25f,0.25f,1), VUI_FONT_SIZE_NORMAL, layer);

    char buf[10];
    snprintf(buf, sizeof(buf), "%i", (int) (intptr_t) v);
    vui_label_create(vui, 0, scrh * 3 / 6, scrw, scrh, buf, vui_color_create(1,0,0,1), VUI_FONT_SIZE_NORMAL, layer);

    const int ok_btn_w = BTN_SZ*2;
    vui_button_create(vui, scrw/2 - ok_btn_w/2, scrh * 3 / 4, ok_btn_w, BTN_SZ, lang(VPI_LANG_OK_BTN), 0, VUI_BUTTON_STYLE_BUTTON, layer, back_to_main_menu, (void *) (intptr_t) layer);

    vui_transition_fade_black_out(vui, 0, 0);
}

void cancel_connect(vui_context_t *vui, int button, void *v)
{
    int layer = (intptr_t) v;
    vanilla_stop();
    vui_transition_fade_layer_out(vui, layer, vpi_menu_main, 0);
}

static void update_battery_information(vui_context_t *vui, int64_t time)
{
    // Get power information
    int64_t current_minute = time / (2 * 1000000); // Update every 2 seconds
    if (menu_game_ctx.last_power_time != current_minute) {
        int percent;
        vui_power_state_t power_state = vui_power_state_get(vui, &percent);

        enum VanillaBatteryStatus status = VANILLA_BATTERY_STATUS_UNKNOWN;

        switch (power_state) {
        case VUI_POWERSTATE_ERROR:
        case VUI_POWERSTATE_UNKNOWN:
            // Unknown status
            status = VANILLA_BATTERY_STATUS_UNKNOWN;
            break;
        case VUI_POWERSTATE_NO_BATTERY:
        case VUI_POWERSTATE_CHARGING:
        case VUI_POWERSTATE_CHARGED:
            // Plugged in
            status = VANILLA_BATTERY_STATUS_CHARGING;
            break;
        case VUI_POWERSTATE_ON_BATTERY:
            // On battery
            status = (percent < 10) ? VANILLA_BATTERY_STATUS_VERY_LOW :
                        (percent < 25) ? VANILLA_BATTERY_STATUS_LOW :
                        (percent < 75) ? VANILLA_BATTERY_STATUS_MEDIUM :
                        (percent < 95) ? VANILLA_BATTERY_STATUS_HIGH :
                        VANILLA_BATTERY_STATUS_FULL;
            break;
        }

        vanilla_set_battery_status(status);

        current_minute = menu_game_ctx.last_power_time;
    }
}

void vpi_display_update(vui_context_t *vui, int64_t time, void *v)
{
    int backend_err = vpi_game_error();
    if (backend_err != VANILLA_SUCCESS) {
        switch (backend_err) {
        case VANILLA_ERR_CONNECTED:
            // Wait for frame before enabling "game mode"
            pthread_mutex_lock(&vpi_decode_loop_mutex);
            if (vpi_present_frame && vpi_present_frame->data[0]) {
                vui_game_mode_set(vui, 1);
				vui_audio_set_enabled(vui, 1);
                vpi_game_set_error(VANILLA_SUCCESS);
            }
            pthread_mutex_unlock(&vpi_decode_loop_mutex);
            break;
        case VANILLA_ERR_DISCONNECTED:
            vui_label_update_text(vui, status_lbl, lang(VPI_LANG_DISCONNECTED));
			vui_audio_set_enabled(vui, 0);
            vui_game_mode_set(vui, 0);
            break;
        case VANILLA_ERR_SHUTDOWN:
            vanilla_stop();
            vpi_menu_main(vui, 0);
            break;
        default:
            vanilla_stop();
            show_error(vui, (void*)(intptr_t) backend_err);
        };
    } else {
        update_battery_information(vui, time);
    }
}

void vpi_menu_game_start(vui_context_t *vui, void *v)
{
    int console = (intptr_t) v;

    vui_reset(vui);

    vui_enable_background(vui, 0);

    menu_game_ctx.last_power_time = 0;

    // Set initial values
    vanilla_set_region(vpi_config.region);

    vpi_console_entry_t *entry = vpi_config.connected_console_entries + console;
    int r = vanilla_start(vpi_config.server_address, entry->bssid, entry->psk);
    if (r != VANILLA_SUCCESS) {
        show_error(vui, (void*)(intptr_t) r);
    } else {
        char buf[100];
        snprintf(buf, sizeof(buf), lang(VPI_LANG_CONNECTING_TO), entry->name);

        int fglayer = vui_layer_create(vui);

        int scrw, scrh;
        vui_get_screen_size(vui, &scrw, &scrh);
        status_lbl = vui_label_create(vui, 0, scrh * 2 / 5, scrw, scrh, buf, vui_color_create(0.5f,0.5f,0.5f,1), VUI_FONT_SIZE_NORMAL, fglayer);

        const int cancel_btn_w = BTN_SZ*3;
        int cancel_btn = vui_button_create(vui, scrw/2 - cancel_btn_w/2, scrh * 3 / 5, cancel_btn_w, BTN_SZ, lang(VPI_LANG_CANCEL_BTN), 0, VUI_BUTTON_STYLE_BUTTON, fglayer, cancel_connect, (void *) (intptr_t) fglayer);

        vui_transition_fade_layer_in(vui, fglayer, 0, 0);

        vpi_game_start(vui);

        vui_start_passive_animation(vui, vpi_display_update, 0);
    }
}

void vpi_menu_game(vui_context_t *vui, void *v)
{
    vpi_menu_start_pipe(vui, 0, vpi_menu_game_start, v, 0, 0);
}
