#include "menu_sudo_warning.h"

#include "lang.h"
#include "menu_common.h"

void vpi_menu_create_sudo_warning(vui_context_t *vui, int layer, vui_button_callback_t ack_action, void *ack_action_userdata, vui_button_callback_t cancel_action, void *cancel_action_userdata)
{
	vui_rect_t bkg_rect;
	int margin;

	vpi_menu_create_background(vui, layer, &bkg_rect, &margin);

	int warning_lbl_w = bkg_rect.w * 3 / 4;
	int warning_lbl_x = bkg_rect.x + bkg_rect.w/2 - warning_lbl_w/2;
	vui_label_create(vui, warning_lbl_x, bkg_rect.y + bkg_rect.h*1/15, warning_lbl_w, bkg_rect.h, lang(VPI_LANG_AUTH_WARNING), vui_color_create(1,0,0,1), VUI_FONT_SIZE_NORMAL, layer);
	vui_label_create(vui, warning_lbl_x, bkg_rect.y + bkg_rect.h*4/20, warning_lbl_w, bkg_rect.h, lang(VPI_LANG_AUTH_WARNING_MESSAGE), vui_color_create(1,1,1,1), VUI_FONT_SIZE_SMALL, layer);

	const int ack_btn_w = BTN_SZ * 4;
	const int cancel_btn_w = BTN_SZ * 3;

	const int btn_x = bkg_rect.x + bkg_rect.w/2 - (ack_btn_w+cancel_btn_w)/2;
	const int btn_y = bkg_rect.y + bkg_rect.h * 3 / 4;
	vui_button_create(vui, btn_x, btn_y, ack_btn_w, BTN_SZ, lang(VPI_LANG_AUTH_WARNING_ACKNOWLEDGE), 0, VUI_BUTTON_STYLE_BUTTON, layer, ack_action, ack_action_userdata);
	vui_button_create(vui, btn_x + ack_btn_w, btn_y, cancel_btn_w, BTN_SZ, lang(VPI_LANG_CANCEL_BTN), 0, VUI_BUTTON_STYLE_BUTTON, layer, cancel_action, cancel_action_userdata);
}
