#include "menu_connection.h"

#ifdef _WIN32
#include <winsock2.h>
typedef uint32_t in_addr_t;
#else
#include <arpa/inet.h>
#endif

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "lang.h"
#include "menu_common.h"
#include "menu_settings.h"
#include "pipemgmt.h"
#include "ui/ui_anim.h"
#include "ui/ui_util.h"

static int fglayer;
static int error_lbl;
static int ip_textedit;

#define MAX_WIRELESS_ENTRIES 4
static char detected_wireless_interfaces[256][256];
static int intf_start = 0;
static int intf_buttons[MAX_WIRELESS_ENTRIES];

static const char *checkmark = 0;//"checkmark.svg";

static vui_callback_t g_continue_callback = 0;
static void *g_continue_callback_data = 0;
static vui_callback_t g_fail_callback = 0;
static void *g_fail_callback_data = 0;

static void return_to_settings(vui_context_t *vui, int btn, void *v)
{
    int layer = (intptr_t) v;

    vui_callback_t cb = g_fail_callback ? g_fail_callback : vpi_menu_settings;
    void *cb_data = g_fail_callback ? g_fail_callback_data : 0;

    vui_transition_fade_layer_out(vui, layer, cb, cb_data);
}

static void return_to_connection(vui_context_t *vui, int btn, void *v)
{
    int layer = (intptr_t) v;

    vui_callback_t cb = g_continue_callback ? g_continue_callback : vpi_menu_connection;
    void *cb_data = g_continue_callback ? g_continue_callback_data : (void *) (intptr_t) 1;

    vui_transition_fade_layer_out(vui, layer, cb, cb_data);
}

static void populate_wireless_intf()
{
    // LINUX ONLY:
    struct dirent *dir;
    DIR *d = opendir("/sys/class/net");
    char buf[1024];
    size_t cur = 0;
    if (d) {
        while ((dir = readdir(d)) != 0) {
            const char *intf = dir->d_name;
            if (intf[0] != '.') {
                snprintf(buf, sizeof(buf), "/sys/class/net/%s/wireless", intf);
                DIR *wifi_check = opendir(buf);
                if (wifi_check) {
                    vui_strncpy(detected_wireless_interfaces[cur], intf, sizeof(detected_wireless_interfaces[cur]));
                    cur++;
                    closedir(wifi_check);
                }
            }
        }
        closedir(d);
    }
}

static void intf_pressed(vui_context_t *vui, int button, void *v)
{
    for (int i = 0; i < MAX_WIRELESS_ENTRIES; i++) {
        if (button == intf_buttons[i]) {
            vpi_config.server_address = VANILLA_ADDRESS_LOCAL;
            vui_strncpy(vpi_config.wireless_interface, detected_wireless_interfaces[intf_start + i], sizeof(vpi_config.wireless_interface));
            vpi_config.connection_setup = 1;
            vpi_stop_pipe();
            vpi_config_save();
            break;
        }
    }
    return_to_connection(vui, button, v);
}

static void update_wireless_buttons(vui_context_t *vui)
{
    for (int i = 0; i < MAX_WIRELESS_ENTRIES; i++) {
        const char *intf = detected_wireless_interfaces[intf_start + i];
        int b = intf_buttons[i];
        if (intf[0] == 0) {
            vui_button_update_visible(vui, b, 0);
        } else {
            vui_button_update_visible(vui, b, 1);
            vui_button_update_text(vui, b, intf);

            int sel = !strcmp(intf, vpi_config.wireless_interface);
            vui_button_update_checked(vui, b, sel);
            vui_button_update_icon(vui, b, sel ? checkmark : 0);
        }
    }
}

static void local_connection_menu(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    int bglayer = vui_layer_create(vui);
    fglayer = vui_layer_create(vui);

    vui_rect_t bkg_rect;
    int margin;
    vpi_menu_create_background(vui, bglayer, &bkg_rect, &margin);

    const int lbl_margin = margin * 6;
    vui_label_create(vui, bkg_rect.x + lbl_margin, bkg_rect.y + lbl_margin, bkg_rect.w - lbl_margin - lbl_margin, bkg_rect.h - lbl_margin - lbl_margin, lang(VPI_LANG_LOCAL_CONNECTION_HELP), vui_color_create(1,1,1,1), VUI_FONT_SIZE_NORMAL, fglayer);

    // Detect wireless interfaces
    memset(detected_wireless_interfaces, 0, sizeof(detected_wireless_interfaces));
    populate_wireless_intf();

    const int button_w = bkg_rect.w/2;
    const int button_h = BTN_SZ*2/3;
    const int button_x = bkg_rect.x+bkg_rect.w/2-button_w/2;
    const int button_y = bkg_rect.y+bkg_rect.h*2/5;
    for (int i = 0; i < MAX_WIRELESS_ENTRIES; i++) {
        int b = vui_button_create(vui, button_x, button_y+button_h*i, button_w, button_h, 0, 0, VUI_BUTTON_STYLE_BUTTON, fglayer, intf_pressed, (void *) (intptr_t) fglayer);
        vui_button_update_font_size(vui, b, VUI_FONT_SIZE_SMALL);
        intf_buttons[i] = b;
    }

    update_wireless_buttons(vui);

    vpi_menu_create_back_button(vui, fglayer, return_to_connection, (void *) (intptr_t) fglayer);
    
    vui_transition_fade_layer_in(vui, fglayer, 0, 0);
}

static void check_ip_address(vui_context_t *vui, int btn, void *v)
{
    char buf[256];
    vui_textedit_get_text(vui, ip_textedit, buf, sizeof(buf));
    in_addr_t ip = inet_addr(buf);

    int valid = (ip != (in_addr_t) -1);

    vui_label_update_visible(vui, error_lbl, !valid);

    if (valid) {
        vpi_config.server_address = ip;
        vpi_config.connection_setup = 1;
        vpi_stop_pipe();
        vpi_config_save();

        return_to_connection(vui, btn, v);
    }
}

static void via_server_connection_menu(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    int bglayer = vui_layer_create(vui);
    fglayer = vui_layer_create(vui);

    vui_rect_t bkg_rect;
    int margin;
    vpi_menu_create_background(vui, bglayer, &bkg_rect, &margin);

    const int lbl_margin = margin * 6;
    vui_label_create(vui, bkg_rect.x + lbl_margin, bkg_rect.y + lbl_margin, bkg_rect.w - lbl_margin - lbl_margin, bkg_rect.h - lbl_margin - lbl_margin, lang(VPI_LANG_SERVER_CONNECTION_HELP), vui_color_create(1,1,1,1), VUI_FONT_SIZE_NORMAL, fglayer);

    const int textedit_w = bkg_rect.w/2;
    const int textedit_h = vui_get_font_height(vui, VUI_FONT_SIZE_NORMAL);
    const int textedit_x = bkg_rect.x + bkg_rect.w/2 - textedit_w/2;

    const char *ip_text;
    if (vpi_config.server_address == VANILLA_ADDRESS_LOCAL) {
        ip_text = 0;
    } else {
        struct in_addr in;
        in.s_addr = vpi_config.server_address;
        ip_text = inet_ntoa(in);
    }

    ip_textedit = vui_textedit_create(vui, textedit_x, bkg_rect.y + bkg_rect.h * 3 / 7, textedit_w, textedit_h, ip_text, VUI_FONT_SIZE_NORMAL, fglayer);

    error_lbl = vui_label_create(vui, textedit_x, bkg_rect.y + bkg_rect.h * 4 / 7, textedit_w, BTN_SZ, "Invalid IP address", vui_color_create(1,0,0,1), VUI_FONT_SIZE_SMALL, fglayer);
    vui_label_update_visible(vui, error_lbl, 0);
    
    vui_button_create(vui, textedit_x, bkg_rect.y + bkg_rect.h * 5 / 7, textedit_w, BTN_SZ, lang(VPI_LANG_OK_BTN), 0, VUI_BUTTON_STYLE_BUTTON, fglayer, check_ip_address, (void *) (intptr_t) fglayer);

    vpi_menu_create_back_button(vui, fglayer, return_to_connection, (void *) (intptr_t) fglayer);

    vui_transition_fade_layer_in(vui, fglayer, 0, 0);
}

void connection_btn_pressed(vui_context_t *vui, int btn, void *v)
{
    vui_callback_t cb = (vui_callback_t) v;
    vui_transition_fade_layer_out(vui, fglayer, cb, 0);
}

void vpi_menu_connection_and_return_to(vui_context_t *vui, int fade_fglayer, vui_callback_t success_callback, void *success_data, vui_callback_t fail_callback, void *fail_data)
{
    vui_reset(vui);

    int bglayer = vui_layer_create(vui);
    fglayer = vui_layer_create(vui);

    vui_rect_t bkg_rect;
    int margin;
    vpi_menu_create_background(vui, bglayer, &bkg_rect, &margin);

    const int lbl_margin = margin * 6;
    vui_label_create(vui, bkg_rect.x + lbl_margin, bkg_rect.y + lbl_margin, bkg_rect.w - lbl_margin - lbl_margin, bkg_rect.h - lbl_margin - lbl_margin, lang(VPI_LANG_CONNECTION_HELP_1), vui_color_create(1,1,1,1), VUI_FONT_SIZE_NORMAL, fglayer);

    const int btn_w = bkg_rect.w/2;
    const int btn_y = bkg_rect.y + bkg_rect.h * 2 / 4;
    const int btn_x = bkg_rect.x + bkg_rect.w/2 - btn_w/2;

    int is_local = vpi_config.server_address == VANILLA_ADDRESS_LOCAL;
    int is_via_server = vpi_config.server_address != VANILLA_ADDRESS_LOCAL;

    const int lbl2_width = bkg_rect.w*2/3;
    const char *lbl2_txt;

#ifdef VANILLA_PIPE_AVAILABLE
    lbl2_txt = lang(VPI_LANG_CONNECTION_HELP_2);
#else
    lbl2_txt = lang(VPI_LANG_NO_LOCAL_AVAILABLE);
    is_local = 0;
#endif

    vui_label_create(vui, bkg_rect.x + bkg_rect.w/2 - lbl2_width/2, bkg_rect.y + lbl_margin + bkg_rect.h/8, lbl2_width, bkg_rect.h - lbl_margin - lbl_margin, lbl2_txt, vui_color_create(0.66f,0.66f,0.66f,1), VUI_FONT_SIZE_SMALL, fglayer);

    int local_btn = vui_button_create(vui, btn_x, btn_y, btn_w, BTN_SZ, lang(VPI_LANG_LOCAL), is_local ? checkmark : 0, VUI_BUTTON_STYLE_BUTTON, fglayer, connection_btn_pressed, local_connection_menu);
    int via_server_btn = vui_button_create(vui, btn_x, btn_y + BTN_SZ, btn_w, BTN_SZ, lang(VPI_LANG_VIA_SERVER), is_via_server ? checkmark : 0, VUI_BUTTON_STYLE_BUTTON, fglayer, connection_btn_pressed, via_server_connection_menu);

    vui_button_update_checkable(vui, local_btn, 1);
    vui_button_update_checkable(vui, via_server_btn, 1);

    vui_button_update_checked(vui, local_btn, is_local);
    vui_button_update_checked(vui, via_server_btn, is_via_server);

#ifndef VANILLA_PIPE_AVAILABLE
    vui_button_update_enabled(vui, local_btn, 0);
#endif

    // Back button
    vpi_menu_create_back_button(vui, fglayer, return_to_settings, (void *) (intptr_t) bglayer);
    
    vui_transition_fade_layer_in(vui, fade_fglayer ? fglayer : bglayer, 0, 0);

    g_continue_callback = success_callback;
    g_continue_callback_data = success_data;
    g_fail_callback = fail_callback;
    g_fail_callback_data = fail_data;
}

void vpi_menu_connection(vui_context_t *vui, void *v)
{
    vpi_menu_connection_and_return_to(vui, (intptr_t) v, 0, 0, 0, 0);
}