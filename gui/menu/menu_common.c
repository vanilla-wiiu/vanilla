#include "menu_common.h"

#include <stdio.h>

#include "config.h"
#include "lang.h"
#include "menu_connection.h"
#include "menu_main.h"
#include "menu_sudo.h"
#include "menu_sync.h"
#include "pipemgmt.h"
#include "ui/ui_anim.h"

int err_fglayer;

void vpi_menu_create_background(vui_context_t *vui, int layer, vui_rect_t *bkg_rect, int *margin)
{
    int scrw, scrh;
    vui_get_screen_size(vui, &scrw, &scrh);

    int m = scrw/75;
    *margin = m;
    bkg_rect->x = m;
    bkg_rect->y = m;
    bkg_rect->w = scrw-m-m;
    bkg_rect->h = scrh-m-m;

    vui_rect_create(vui, bkg_rect->x, bkg_rect->y, bkg_rect->w, bkg_rect->h, scrh*1/10, vui_color_create(0, 0, 0, 0.66f), layer);
}

void vpi_menu_create_back_button(vui_context_t *vui, int layer, vui_button_callback_t action, void *action_data)
{
    int cbtn = vui_button_create(vui, 0, 0, BTN_SZ, BTN_SZ, lang(VPI_LANG_BACK), 0, VUI_BUTTON_STYLE_CORNER, layer, action, action_data);
    vui_button_set_cancel(vui, cbtn);
}

struct vpi_menu_start_pipe_data {
    vui_callback_t success_action;
    void *success_data;
    vui_callback_t cancel_action;
    void *cancel_data;
};

void vpi_menu_start_pipe_thunk(vui_context_t *vui, void *data)
{
    struct vpi_menu_start_pipe_data *p = (struct vpi_menu_start_pipe_data *) data;
    vpi_menu_start_pipe(vui, 1, p->success_action, p->success_data, p->cancel_action, p->cancel_data);
    free(p);
}

void back_to_main_menu_action(vui_context_t *vui, int btn, void *v)
{
    vui_transition_fade_layer_out(vui, err_fglayer, vpi_menu_main, (void *) (intptr_t) 1);
}

void vpi_menu_exit_pipe_thunk(vui_context_t *vui, void *data)
{
    struct vpi_menu_start_pipe_data *p = (struct vpi_menu_start_pipe_data *) data;
    free(p);
    vpi_menu_main(vui, (void *) (intptr_t) 1);
}

void jump_to_connection_setup(vui_context_t *vui, int fade_fglayer, vui_callback_t success_action, void *success_data, vui_callback_t cancel_action, void *cancel_data)
{
    struct vpi_menu_start_pipe_data *p = malloc(sizeof(struct vpi_menu_start_pipe_data));
    p->success_action = success_action;
    p->success_data = success_data;
    p->cancel_action = cancel_action;
    p->cancel_data = cancel_data;
    vpi_menu_connection_and_return_to(vui, fade_fglayer, vpi_menu_start_pipe_thunk, p, vpi_menu_exit_pipe_thunk, p);
}

void vpi_menu_start_pipe(vui_context_t *vui, int fade_fglayer, vui_callback_t success_action, void *success_data, vui_callback_t cancel_action, void *cancel_data)
{
    if (!vpi_config.connection_setup) {
        jump_to_connection_setup(vui, fade_fglayer, success_action, success_data, cancel_action, cancel_data);
    } else if (vpi_config.server_address == VANILLA_ADDRESS_LOCAL) {
#ifdef VANILLA_PIPE_AVAILABLE
        int r = vpi_start_pipe();
        if (r == VANILLA_SUCCESS) {
            success_action(vui, success_data);
#ifdef VANILLA_POLKIT_AVAILABLE
		} else if (r == VANILLA_REQUIRES_PASSWORD_HANDLING) {
			vpi_menu_sudo(vui, success_action, success_data, cancel_action, cancel_data, fade_fglayer);
#endif
        } else {
            if (cancel_action) {
                cancel_action(vui, cancel_data);
            }
            err_fglayer = vpi_menu_show_error(vui, r, 0, back_to_main_menu_action, 0);
        }
#else
        // Local is not available on this platform
        jump_to_connection_setup(vui, fade_fglayer, success_action, success_data, cancel_action, cancel_data);
#endif
    } else {
        // Using a middleman server, proceed as usual
        success_action(vui, success_data);
    }
}

int vpi_menu_show_error(vui_context_t *vui, int status, int fade_fglayer, vui_button_callback_t ok_action, void *ok_data)
{
    vui_reset(vui);

    int bglayer = vui_layer_create(vui);

    vui_rect_t bkg_rect;
    int margin;
    vpi_menu_create_background(vui, bglayer, &bkg_rect, &margin);

    int fglayer = vui_layer_create(vui);

    vui_label_create(vui, bkg_rect.x, bkg_rect.y + bkg_rect.h/4, bkg_rect.w, bkg_rect.h, lang(VPI_LANG_SYNC_FAILED), vui_color_create(1,1,1,1), VUI_FONT_SIZE_NORMAL, fglayer);

    char buf[20];
    snprintf(buf, sizeof(buf), "%i", status);
    vui_label_create(vui, bkg_rect.x, bkg_rect.y + bkg_rect.h/2, bkg_rect.w, bkg_rect.h, buf, vui_color_create(1,0,0,1), VUI_FONT_SIZE_SMALL, fglayer);

    vui_button_create(vui, bkg_rect.x + bkg_rect.w/2 - BTN_SZ, bkg_rect.y + bkg_rect.h * 3 / 4, BTN_SZ*2, BTN_SZ, lang(VPI_LANG_OK_BTN), 0, VUI_BUTTON_STYLE_BUTTON, fglayer, ok_action, ok_data);

    if (fade_fglayer)
        vui_transition_fade_layer_in(vui, fglayer, 0, 0);
    else
        vui_transition_fade_black_out(vui, 0, 0);

    return bglayer;
}

static void do_quit(vui_context_t *vui, void *v)
{
    vui_reset(vui);
    vui_enable_background(vui, 0);
    vui_quit(vui);
}

void vpi_menu_quit_vanilla(vui_context_t *vui)
{
    vui_transition_fade_black_in(vui, do_quit, 0);
}
