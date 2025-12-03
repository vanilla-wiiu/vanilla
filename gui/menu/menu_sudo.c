#include "menu_sudo.h"

#ifdef VANILLA_PIPE_AVAILABLE

#include "config.h"
#include "lang.h"
#include "menu_common.h"
#include "menu_main.h"
#include "menu_sudo_warning.h"
#include "ui/ui_anim.h"
#include "pipemgmt.h"

static int fglayer;
static int textedit;
static vui_callback_t g_success_action;
static void *g_success_data;
static vui_callback_t g_cancel_action;
static void *g_cancel_data;

static int ok_btn;
static int cancel_btn;
static int err_lbl;
static int remember_btn;
static int warning_layer;

static int remember_warning_displayed = 0;

static void return_to_main(vui_context_t *vui, int btn, void *v)
{
    int layer = (intptr_t) v;
	vpi_cancel_pw();
	if (g_cancel_action) {
		g_cancel_action(vui, g_cancel_data);
	}
    vui_transition_fade_layer_out(vui, layer, vpi_menu_main, (void *) (intptr_t) 1);
}

static void show_error(vui_context_t *vui, void *v)
{
	vpi_menu_show_error(vui, (int) (intptr_t) v, 1, return_to_main, (void *) (intptr_t) fglayer);
}

static void pw_callback(int result, void *v)
{
	vui_context_t *vui = (vui_context_t *) v;

	switch (result) {
	case VPI_POLKIT_RESULT_SUCCESS:
	{
		int r = vpi_start_epilog();

		if (vui_button_get_checked(vui, remember_btn)) {
			vanilla_install_polkit(vpi_config.server_address);
		}

		if (r == VANILLA_SUCCESS) {
			vui_transition_fade_layer_out(vui, fglayer, g_success_action, g_success_data);
		} else {
			vui_transition_fade_layer_out(vui, fglayer, show_error, (void *) (intptr_t) r);
		}
		break;
	}
	case VPI_POLKIT_RESULT_RETRY:
		vui_label_update_text(vui, err_lbl, lang(VPI_LANG_AUTH_FAIL));
		vui_textedit_update_text(vui, textedit, "");

		vui_button_update_enabled(vui, ok_btn, 1);
		vui_button_update_enabled(vui, cancel_btn, 1);
		vui_button_update_enabled(vui, remember_btn, 1);
		vui_textedit_update_enabled(vui, textedit, 1);
		break;
	case VPI_POLKIT_RESULT_FAIL:
		vui_transition_fade_layer_out(vui, fglayer, show_error, (void *) (intptr_t) VANILLA_PASSWORD_FAILED);
		break;
	}
}

static void submit_pw(vui_context_t *vui, int btn, void *v)
{
    char buf[1024];
    vui_textedit_get_text(vui, textedit, buf, sizeof(buf));
	vpi_submit_pw(buf, pw_callback, vui);

	vui_button_update_enabled(vui, ok_btn, 0);
	vui_button_update_enabled(vui, cancel_btn, 0);
	vui_button_update_enabled(vui, remember_btn, 0);
	vui_textedit_update_enabled(vui, textedit, 0);
	vui_label_update_text(vui, err_lbl, "");
}

static void hide_warning(vui_context_t *vui, int enable)
{
	vui_layer_set_enabled(vui, warning_layer, 0);
	vui_layer_set_enabled(vui, fglayer, 1);
	vui_button_update_checked(vui, remember_btn, enable);
	if (enable) remember_warning_displayed = 1;
}

static void hide_warning_and_ack(vui_context_t *vui, int btn, void *v)
{
	hide_warning(vui, 1);
}

static void hide_warning_and_cancel(vui_context_t *vui, int btn, void *v)
{
	hide_warning(vui, 0);
}

static void dont_ask_again_btn_pressed(vui_context_t *vui, int btn, void *userdata)
{
	int v = !vui_button_get_checked(vui, btn);

	if (v == 1 && !remember_warning_displayed) {
		vui_layer_set_enabled(vui, warning_layer, 1);
		vui_layer_set_enabled(vui, fglayer, 0);
	} else {
		vui_button_update_checked(vui, btn, v);
	}
}

void vpi_menu_sudo(vui_context_t *vui, vui_callback_t success_action, void *success_data, vui_callback_t cancel_action, void *cancel_data, int fade_fglayer)
{
    vui_reset(vui);

    int bglayer = vui_layer_create(vui);
    fglayer = vui_layer_create(vui);

    vui_rect_t bkg_rect;
    int margin;
    vpi_menu_create_background(vui, bglayer, &bkg_rect, &margin);

	vui_label_create(vui, bkg_rect.x, bkg_rect.y + bkg_rect.h*1/10, bkg_rect.w, bkg_rect.h, lang(VPI_LANG_SUDO_HELP_1), vui_color_create(1,1,1,1), VUI_FONT_SIZE_NORMAL, fglayer);
	vui_label_create(vui, bkg_rect.x, bkg_rect.y + bkg_rect.h*3/10, bkg_rect.w, bkg_rect.h, lang(VPI_LANG_SUDO_HELP_2), vui_color_create(1,1,1,1), VUI_FONT_SIZE_SMALL, fglayer);

    const int textedit_w = bkg_rect.w / 2;
    const int textedit_h = vui_get_font_height(vui, VUI_FONT_SIZE_NORMAL);
    textedit = vui_textedit_create(vui, bkg_rect.x + bkg_rect.w/2 - textedit_w/2, bkg_rect.y + bkg_rect.h * 4/10, textedit_w, textedit_h, "", VUI_FONT_SIZE_NORMAL, 1, fglayer);

	g_success_action = success_action;
	g_success_data = success_data;
	g_cancel_action = cancel_action;
	g_cancel_data = cancel_data;

	err_lbl = vui_label_create(vui, bkg_rect.x, bkg_rect.y + bkg_rect.h*11/20, bkg_rect.w, bkg_rect.h, "", vui_color_create(1,0,0,1), VUI_FONT_SIZE_TINY, fglayer);

    static const int ok_btn_w = BTN_SZ * 3;
    static const int cancel_btn_w = BTN_SZ * 3;

	remember_btn = vui_button_create(vui, bkg_rect.w/2 - textedit_w/2, bkg_rect.y + bkg_rect.h * 6 / 10, textedit_w, BTN_SZ * 3 / 4, lang(VPI_LANG_AUTH_DONT_ASK_AGAIN), 0, VUI_BUTTON_STYLE_BUTTON, fglayer, dont_ask_again_btn_pressed, 0);
	vui_button_update_checkable(vui, remember_btn, 1);

    ok_btn = vui_button_create(vui, bkg_rect.x + bkg_rect.w/2 - (ok_btn_w + cancel_btn_w)/2, bkg_rect.y + bkg_rect.h * 3 / 4, ok_btn_w, BTN_SZ, lang(VPI_LANG_OK_BTN), 0, VUI_BUTTON_STYLE_BUTTON, fglayer, submit_pw, 0);
    cancel_btn = vui_button_create(vui, bkg_rect.x + bkg_rect.w/2 - (ok_btn_w + cancel_btn_w)/2 + ok_btn_w, bkg_rect.y + bkg_rect.h * 3 / 4, cancel_btn_w, BTN_SZ, lang(VPI_LANG_CANCEL_BTN), 0, VUI_BUTTON_STYLE_BUTTON, fglayer, return_to_main,  (void *) (intptr_t) fglayer);

	warning_layer = vui_layer_create(vui);
	vpi_menu_create_sudo_warning(vui, warning_layer, hide_warning_and_ack, 0, hide_warning_and_cancel, 0);
	vui_layer_set_enabled(vui, warning_layer, 0);

	vui_transition_fade_layer_in(vui, fade_fglayer ? fglayer : bglayer, 0, 0);
}

#endif // VANILLA_PIPE_AVAILABLE
