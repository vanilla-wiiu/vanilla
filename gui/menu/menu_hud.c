#include "menu_hud.h"

#include <vanilla.h>

#include "lang.h"
#include "menu.h"
#include "menu_game.h"
#include "platform.h"
#include "ui/ui_anim.h"
#include "menu_common.h"

#define BUTTON_COUNT 5

static int bglayer;

static void do_take_screenshot(vui_context_t *vui, int btn, void *v)
{
    vpi_take_screenshot();
    vui_hud_enable(vui, 0);
}

static void do_toggle_recording(vui_context_t *vui, int btn, void *v)
{
    vpi_toggle_recording();
    vui_hud_enable(vui, 0);
}

static void do_nfc(vui_context_t *vui, int btn, void *v)
{
    vpi_show_toast("Coming soon!");
}

static void do_disconnect(vui_context_t *vui, int btn, void *v)
{
    vpi_game_shutdown();
    vui_hud_enable(vui, 0);
}

static void do_hud_disable(vui_context_t *vui, void *v)
{
    vui_hud_enable(vui, 0);
}

static void do_blow_mic(vui_context_t *vui, int btn, void *v)
{
    vanilla_send_mic_blow();
    vui_hud_enable(vui, 0);
}

void vpi_menu_hud_close(vui_context_t *vui, void *v)
{
    vui_transition_fade_layer_out(vui, bglayer, do_hud_disable, NULL);
}

static void do_resume(vui_context_t *vui, int btn, void *v)
{
    vpi_menu_hud_close(vui, NULL);
}

void vpi_menu_hud(vui_context_t *vui, void *v)
{
    vui_reset_without_game_mode(vui);

    int sw, sh;
    vui_get_screen_size(vui, &sw, &sh);

    bglayer = vui_layer_create(vui);
    vui_rect_create(vui, 0, 0, sw, sh, 0, vui_color_create(0, 0, 0, 0.75f), bglayer);

    int fglayer = vui_layer_create(vui);

    int btn_area_x = sw * 1 / 20;
    int btn_area_top = sh * 3 / 10;
    int btn_area_bottom = sh * 1 / 10;
    int btn_area_w = sw - btn_area_x - btn_area_x;
    int btn_area_h = sh - btn_area_top - btn_area_bottom;

    vui_image_create(vui, sw/2-btn_area_top/2, 0, btn_area_top, btn_area_top, "vanilla.png", fglayer);

    struct {
        int button_text;
        const char *icon;
        vui_button_callback_t callback;
        void *callback_data;
    } buttons[BUTTON_COUNT] = {
        {
            .button_text = !vpi_decode_is_recording() ? VPI_LANG_START_RECORDING_VIDEO : VPI_LANG_STOP_RECORDING_VIDEO,
            .icon = "recording_icon.svg",
            .callback = do_toggle_recording
        },
        {
            .button_text = VPI_LANG_TAKE_SCREENSHOT,
            .icon = "screenshot_icon.svg",
            .callback = do_take_screenshot
        },
        {
            .button_text = VPI_LANG_BLOW_MIC,
            .icon = NULL,
            .callback = do_blow_mic
        },
        {
            .button_text = VPI_LANG_NFC,
            .icon = NULL,
            .callback = do_nfc
        },
        {
            .button_text = VPI_LANG_DISCONNECT,
            .icon = NULL,
            .callback = do_disconnect
        },
    };

    const int button_columns = 2;
    const int button_width = btn_area_w/button_columns;
    const int button_height = btn_area_h/((BUTTON_COUNT + button_columns - 1)/button_columns);
    for (int i = 0; i < BUTTON_COUNT; i++) {
        // If button count is odd and this is the last button, allow it to span
        // full screen width
        int use_width = button_width;

        if (i == BUTTON_COUNT-1 && BUTTON_COUNT%2 != 0) {
            use_width = btn_area_w;
        }

        vui_button_create(
            vui,
            btn_area_x + button_width * (i % button_columns),
            btn_area_top + button_height * (i / button_columns),
            use_width,
            button_height,
            lang(buttons[i].button_text),
            buttons[i].icon,
            VUI_BUTTON_STYLE_BUTTON,
            fglayer,
            buttons[i].callback,
            buttons[i].callback_data
        );
    }

    int corner_btn_sz = BTN_SZ;
    vui_button_create(
        vui,
        sw-corner_btn_sz,
        0,
        corner_btn_sz,
        corner_btn_sz,
        lang(VPI_LANG_RESUME),
        NULL, // "settings.png",
        VUI_BUTTON_STYLE_CORNER,
        fglayer,
        do_resume,
        NULL
    );

    // vui_label_create(vui, 0, sh / 10, sw, sh, lang(VPI_LANG_VANILLA_MENU), vui_color_create(1, 1, 1, 1), VUI_FONT_SIZE_NORMAL, fglayer);

    vui_transition_fade_layer_in(vui, bglayer, NULL, NULL);

    // HACK: Calling vui_reset removes this, but we need to keep it going
    vui_start_passive_animation(vui, vpi_display_update, 0);
}
