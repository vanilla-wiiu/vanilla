#include "menu_gamepad.h"

#include "config.h"
#include "lang.h"
#include "menu/menu_common.h"
#include "menu/menu_settings.h"
#include "ui/ui_anim.h"
#include "ui/ui_priv.h"
#include "ui/ui_keyboard.h"
#include "lib/vanilla.h"


#define MAIN_MENU_ENTRIES 3

static const uint32_t gICON_COLOUR = 0x99999999;

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

static int find_current_keybind(const vui_context_t *ctx, int btn)
{
  for(int i=0; i < ctx->key_map_sz; i++){
    if(ctx->key_map[i] == btn)
      return i;
  }
  return -1;
}

static void return_to_gamepad(vui_context_t *vui, int btn, void *v)
{
  int layer = (intptr_t) v;
  vui_transition_fade_layer_out(vui, layer, vpi_menu_gamepad, 0);
}

static void transition_to_keybind(vui_context_t *vui, int btn, void *v)
{
  int layer = (intptr_t) v;
  vui_transition_fade_layer_out(vui, layer, vpi_menu_key_bindings, 0);
}

static void vpi_bind_draw_callback(vui_context_t *ctx, vui_button_t *button, void *userdata)
{
  if(ctx->bind_mode){
    button->icon_mod += 0x020202; 
    return;
  }

  button->icon_mod = gICON_COLOUR;

  for(int i = 0; i < ctx->button_count; i++){
    if(ctx->buttons[i].style != VUI_BUTTON_STYLE_SMALL) continue;
    vui_button_update_icon(ctx, i, vui_get_keyicon_from_scancode(find_current_keybind(ctx, (int)(intptr_t)ctx->buttons[i].onclick_data)));
  }
  
  //Unset the draw callback
  vui_button_update_draw_handler(ctx, (int) (intptr_t) userdata, NULL, NULL);
}

static void vpi_bind_callback(vui_context_t *ctx, int button, void *userdata)
{
  if(ctx->bind_mode) return;
  ctx->bind_mode = (int) (intptr_t)userdata;
  vui_button_update_draw_handler(ctx, button, vpi_bind_draw_callback, (void *)(uintptr_t)button);
}

void vpi_menu_key_bindings(vui_context_t *vui, void *v)
{
  vui_reset(vui);

  int layer = vui_layer_create(vui);

  int SCREEN_WIDTH, SCREEN_HEIGHT;
  vui_get_screen_size(vui, &SCREEN_WIDTH, &SCREEN_HEIGHT);

  const int img_w = SCREEN_WIDTH / 2;
  const int img_h = SCREEN_HEIGHT / 2;
  const int img_x = SCREEN_WIDTH / 2 - img_w / 2;
  const int img_y = SCREEN_HEIGHT / 2 - img_h / 2;
  vui_image_create(vui, img_x, img_y, img_w, img_h, "gamepad.svg", layer);

  const int btn_width = 50;
  const int btn_height = 50;
  const int padding = 5;
  
  {
    // SHOULDER BUTTONS
    const char *button_icon;
    button_icon = vui_get_keyicon_from_scancode(find_current_keybind(vui, VANILLA_BTN_ZL));
    int zl_button = vui_button_create(vui, img_x, img_y - btn_height * 2 - padding, btn_width, btn_height, NULL, button_icon, VUI_BUTTON_STYLE_SMALL, layer, vpi_bind_callback, (void *) (intptr_t)VANILLA_BTN_ZL);
    vui_button_update_icon_mod(vui, zl_button, gICON_COLOUR);
    button_icon = vui_get_keyicon_from_scancode(find_current_keybind(vui, VANILLA_BTN_L));
    int l_button = vui_button_create(vui, img_x, img_y - btn_height - padding, btn_width, btn_height, NULL, button_icon, VUI_BUTTON_STYLE_SMALL, layer, vpi_bind_callback, (void *) (intptr_t)VANILLA_BTN_L);
    vui_button_update_icon_mod(vui, l_button, gICON_COLOUR);
    button_icon = vui_get_keyicon_from_scancode(find_current_keybind(vui, VANILLA_BTN_ZR));
    int zr_button = vui_button_create(vui, img_x + img_w - btn_width, img_y - btn_height * 2 - padding, btn_width, btn_height, NULL, button_icon, VUI_BUTTON_STYLE_SMALL, layer, vpi_bind_callback, (void *) (intptr_t)VANILLA_BTN_ZR);
    vui_button_update_icon_mod(vui, zr_button, gICON_COLOUR);
    button_icon = vui_get_keyicon_from_scancode(find_current_keybind(vui, VANILLA_BTN_R));
    int r_button = vui_button_create(vui, img_x + img_w - btn_width, img_y - btn_height - padding, btn_width, btn_height, NULL, button_icon, VUI_BUTTON_STYLE_SMALL, layer, vpi_bind_callback, (void *) (intptr_t)VANILLA_BTN_R);
    vui_button_update_icon_mod(vui, r_button, gICON_COLOUR);

    const int dpad_x_ref = img_w / 4 - btn_width / 2;
    const int dpad_y_ref = img_y + img_h / 2;
    
    const int abxy_x_ref = img_x + img_w + img_w / 4 - btn_width / 2;
    const int abxy_y_ref = dpad_y_ref;
    
    // DPAD
    button_icon = vui_get_keyicon_from_scancode(find_current_keybind(vui, VANILLA_BTN_UP));
    int dup_button = vui_button_create(vui, dpad_x_ref, dpad_y_ref - btn_height * 1.5 + padding, btn_width, btn_height, NULL, button_icon, VUI_BUTTON_STYLE_SMALL, layer, vpi_bind_callback, (void *) (intptr_t)VANILLA_BTN_UP);
    vui_button_update_icon_mod(vui, dup_button, gICON_COLOUR);
    button_icon = vui_get_keyicon_from_scancode(find_current_keybind(vui, VANILLA_BTN_DOWN));
    int dwn_button = vui_button_create(vui, dpad_x_ref, dpad_y_ref + btn_height / 2 - padding, btn_width, btn_height, NULL, button_icon, VUI_BUTTON_STYLE_SMALL, layer, vpi_bind_callback, (void *) (intptr_t)VANILLA_BTN_DOWN);
    vui_button_update_icon_mod(vui, dwn_button, gICON_COLOUR);
    button_icon = vui_get_keyicon_from_scancode(find_current_keybind(vui, VANILLA_BTN_LEFT));
    int dlf_button = vui_button_create(vui, dpad_x_ref - btn_width, dpad_y_ref - btn_height / 2, btn_width, btn_height, NULL, button_icon, VUI_BUTTON_STYLE_SMALL, layer, vpi_bind_callback, (void *) (intptr_t)VANILLA_BTN_LEFT);
    vui_button_update_icon_mod(vui, dlf_button, gICON_COLOUR);
    button_icon = vui_get_keyicon_from_scancode(find_current_keybind(vui, VANILLA_BTN_RIGHT));
    int drg_button = vui_button_create(vui, dpad_x_ref + btn_width, dpad_y_ref - btn_height / 2, btn_width, btn_height, NULL, button_icon, VUI_BUTTON_STYLE_SMALL, layer, vpi_bind_callback, (void *) (intptr_t)VANILLA_BTN_RIGHT);
    vui_button_update_icon_mod(vui, drg_button, gICON_COLOUR);

    // ABXY
    button_icon = vui_get_keyicon_from_scancode(find_current_keybind(vui, VANILLA_BTN_X));
    int x_button = vui_button_create(vui, abxy_x_ref, abxy_y_ref - btn_height * 1.5 + padding, btn_width, btn_height, NULL, button_icon, VUI_BUTTON_STYLE_SMALL, layer, vpi_bind_callback, (void *) (intptr_t)VANILLA_BTN_X);
    vui_button_update_icon_mod(vui, x_button, gICON_COLOUR);
    button_icon = vui_get_keyicon_from_scancode(find_current_keybind(vui, VANILLA_BTN_B));
    int b_button = vui_button_create(vui, abxy_x_ref, abxy_y_ref + btn_height / 2 - padding, btn_width, btn_height, NULL, button_icon, VUI_BUTTON_STYLE_SMALL, layer, vpi_bind_callback, (void *) (intptr_t)VANILLA_BTN_B);
    vui_button_update_icon_mod(vui, b_button, gICON_COLOUR);
    button_icon = vui_get_keyicon_from_scancode(find_current_keybind(vui, VANILLA_BTN_Y));
    int y_button = vui_button_create(vui, abxy_x_ref - btn_width, abxy_y_ref - btn_height / 2, btn_width, btn_height, NULL, button_icon, VUI_BUTTON_STYLE_SMALL, layer, vpi_bind_callback, (void *) (intptr_t)VANILLA_BTN_Y);
    vui_button_update_icon_mod(vui, y_button, gICON_COLOUR);
    button_icon = vui_get_keyicon_from_scancode(find_current_keybind(vui, VANILLA_BTN_A));
    int a_button = vui_button_create(vui, abxy_x_ref + btn_width, abxy_y_ref - btn_height / 2, btn_width, btn_height, NULL, button_icon, VUI_BUTTON_STYLE_SMALL, layer, vpi_bind_callback, (void *) (intptr_t)VANILLA_BTN_A);
    vui_button_update_icon_mod(vui, a_button, gICON_COLOUR);

    const int plus_minus_y_ref = abxy_y_ref + btn_height * 1.5;

    button_icon = vui_get_keyicon_from_scancode(find_current_keybind(vui, VANILLA_BTN_PLUS));
    int plus_button = vui_button_create(vui, abxy_x_ref - btn_width, plus_minus_y_ref, btn_width, btn_height, NULL, button_icon, VUI_BUTTON_STYLE_SMALL, layer, vpi_bind_callback, (void *) (intptr_t)VANILLA_BTN_PLUS);
    vui_button_update_icon_mod(vui, plus_button, gICON_COLOUR);
    button_icon = vui_get_keyicon_from_scancode(find_current_keybind(vui, VANILLA_BTN_MINUS));
    int minus_button = vui_button_create(vui, abxy_x_ref - btn_width, plus_minus_y_ref + btn_height + padding, btn_width, btn_height, NULL, button_icon, VUI_BUTTON_STYLE_SMALL, layer, vpi_bind_callback, (void *) (intptr_t)VANILLA_BTN_MINUS);
    vui_button_update_icon_mod(vui, minus_button, gICON_COLOUR);

    button_icon = vui_get_keyicon_from_scancode(find_current_keybind(vui, VANILLA_BTN_HOME));
    int home_button = vui_button_create(vui, img_w - btn_width / 2, img_y + img_h + padding, btn_width, btn_height, NULL, button_icon, VUI_BUTTON_STYLE_SMALL, layer, vpi_bind_callback, (void *)(intptr_t)VANILLA_BTN_HOME);
    vui_button_update_icon_mod(vui, home_button, gICON_COLOUR);
    button_icon = vui_get_keyicon_from_scancode(find_current_keybind(vui, VANILLA_BTN_TV));
    int tv_button = vui_button_create(vui, img_w + btn_width * 1.5, img_y + img_h + padding, btn_width, btn_height, NULL, button_icon, VUI_BUTTON_STYLE_SMALL, layer, vpi_bind_callback, (void*)(intptr_t)VANILLA_BTN_TV);
    vui_button_update_icon_mod(vui, tv_button, gICON_COLOUR);

  }

  vpi_menu_create_back_button(vui, layer, return_to_gamepad, (void *) (intptr_t) layer);
}

void vpi_menu_gamepad(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    int layer = vui_layer_create(vui);

    int SCREEN_WIDTH, SCREEN_HEIGHT;
    vui_get_screen_size(vui, &SCREEN_WIDTH, &SCREEN_HEIGHT);

    int list_item_width = SCREEN_WIDTH - BTN_SZ - BTN_SZ;
    int list_item_height = (SCREEN_HEIGHT - BTN_SZ - BTN_SZ) / MAIN_MENU_ENTRIES;

    int swap_abxy_button = vui_button_create(vui, BTN_SZ, BTN_SZ + list_item_height * 0, list_item_width, list_item_height, lang(VPI_LANG_SWAP_ABXY_BUTTONS), NULL, VUI_BUTTON_STYLE_LIST, layer, vpi_menu_toggle_swap_abxy, 0);
    vui_button_update_checkable(vui, swap_abxy_button, 1);
    vui_button_update_checked(vui, swap_abxy_button, vpi_config.swap_abxy);

    int keyboard_bind_button = vui_button_create(vui, BTN_SZ, BTN_SZ + list_item_height * 1, list_item_width, list_item_height, "Keyboard Controls", NULL, VUI_BUTTON_STYLE_LIST, layer, transition_to_keybind, (void *)(intptr_t)layer); 

    // Back button
    vpi_menu_create_back_button(vui, layer, return_to_settings, (void *) (intptr_t) layer);

    // More button
    //vui_button_create(vui, scrw-BTN_SZ, 0, BTN_SZ, BTN_SZ, lang(VPI_LANG_MORE), 0, VUI_BUTTON_STYLE_CORNER, layer, return_to_settings, (void *) (intptr_t) layer);

    vui_transition_fade_layer_in(vui, layer, 0, 0);
}