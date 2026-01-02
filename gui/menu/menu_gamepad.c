#include "menu_gamepad.h"

#include "config.h"
#include "lang.h"
#include "menu/menu_common.h"
#include "menu/menu_settings.h"
#include "ui/ui_anim.h"
#include "ui/ui_priv.h"
#include "lib/vanilla.h"

#include <stdio.h>
#include <string.h>

#define MAIN_MENU_ENTRIES 3

static void return_to_settings(vui_context_t *vui, int btn, void *v)
{
    int layer = (intptr_t) v;
    vui_transition_fade_layer_out(vui, layer, vpi_menu_settings, 0);
}

void vpi_menu_toggle_swap_abxy(vui_context_t *vui, int btn, void *v)
{
    vpi_config.swap_abxy = !vpi_config.swap_abxy;
    vpi_config_save();

    vui_button_update_checked(vui, btn, vpi_config.swap_abxy);
}


static void vpi_bind_callback(vui_context_t *ctx, int button, void *userdata)
{
  ctx->bind_mode = !ctx->bind_mode;
}

static int find_current_key_bind(const vui_context_t *ctx, enum VanillaGamepadButtons btn)
{
  for(int i=0; i < ctx->key_map_sz; i++){
    if(ctx->key_map[i] == btn)
      return i;
  }
  return -1;
}

void vpi_menu_gamepad(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    int layer = vui_layer_create(vui);

    int scrw, scrh;
    vui_get_screen_size(vui, &scrw, &scrh);

    // int img_w = scrw / 2;
    // int img_h = scrh / 2;
    // vui_image_create(vui, scrw/2-img_w/2, scrh/2-img_h/2 + 50, img_w, img_h, "gamepad.svg", layer);

    // int tmp_lbl = vui_label_create(vui, scrw/4, scrh/8, scrw/2, scrh, "Controller remapping is not yet implemented in this version, please come back soon...", vui_color_create(0.25f,0.25f,0.25f,1), VUI_FONT_SIZE_SMALL, layer);

    int SCREEN_WIDTH, SCREEN_HEIGHT;
    vui_get_screen_size(vui, &SCREEN_WIDTH, &SCREEN_HEIGHT);

    int list_item_width = SCREEN_WIDTH - BTN_SZ - BTN_SZ;
    int list_item_height = (SCREEN_HEIGHT - BTN_SZ - BTN_SZ) / MAIN_MENU_ENTRIES;

    int swap_abxy_button = vui_button_create(vui, BTN_SZ, BTN_SZ + list_item_height * 0, list_item_width, list_item_height, lang(VPI_LANG_SWAP_ABXY_BUTTONS), NULL, VUI_BUTTON_STYLE_LIST, layer, vpi_menu_toggle_swap_abxy, 0);
    vui_button_update_checkable(vui, swap_abxy_button, 1);
    vui_button_update_checked(vui, swap_abxy_button, vpi_config.swap_abxy);

    {
      char bind_text[MAX_BUTTON_TEXT];

      int current_bind = find_current_key_bind(vui, VANILLA_BTN_A);
      char current_bind_txt[32] = "N/A";
      if(current_bind >= 0);
	sprintf(current_bind_txt, "%d", current_bind);
      
      snprintf(bind_text, MAX_BUTTON_TEXT, "Gamepad A: %s", current_bind_txt);
      int bind_gamepad_a = vui_button_create(vui, BTN_SZ, BTN_SZ + list_item_height * 1, list_item_width, list_item_height, bind_text, NULL, VUI_BUTTON_STYLE_LIST, layer, vpi_bind_callback, 0);
    }
    

    // Back button
    vpi_menu_create_back_button(vui, layer, return_to_settings, (void *) (intptr_t) layer);

    // More button
    //vui_button_create(vui, scrw-BTN_SZ, 0, BTN_SZ, BTN_SZ, lang(VPI_LANG_MORE), 0, VUI_BUTTON_STYLE_CORNER, layer, return_to_settings, (void *) (intptr_t) layer);

    vui_transition_fade_layer_in(vui, layer, 0, 0);
}
