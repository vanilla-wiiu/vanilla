#include "ui/ui_keyboard.h"

#include <SDL2/SDL_scancode.h>

const char *VUI_KEYBOARD_ICONS[VUI_KEYBOARD_ICON_COUNT] = {
  "keys/keyboard_a.svg",
  "keys/keyboard_b.svg",
  "keys/keyboard_c.svg",
  "keys/keyboard_d.svg",
  "keys/keyboard_e.svg",
  "keys/keyboard_f.svg",
  "keys/keyboard_g.svg",
  "keys/keyboard_h.svg",
  "keys/keyboard_i.svg",
  "keys/keyboard_j.svg",
  "keys/keyboard_k.svg",
  "keys/keyboard_l.svg",
  "keys/keyboard_m.svg",
  "keys/keyboard_n.svg",
  "keys/keyboard_o.svg",
  "keys/keyboard_p.svg",
  "keys/keyboard_q.svg",
  "keys/keyboard_r.svg",
  "keys/keyboard_s.svg",
  "keys/keyboard_t.svg",
  "keys/keyboard_u.svg",
  "keys/keyboard_v.svg",
  "keys/keyboard_w.svg",
  "keys/keyboard_x.svg",
  "keys/keyboard_y.svg",
  "keys/keyboard_z.svg",
  "keys/keyboard_0.svg",
  "keys/keyboard_1.svg",
  "keys/keyboard_2.svg",
  "keys/keyboard_3.svg",
  "keys/keyboard_4.svg",
  "keys/keyboard_5.svg",
  "keys/keyboard_6.svg",
  "keys/keyboard_7.svg",
  "keys/keyboard_8.svg",
  "keys/keyboard_9.svg",
  "keys/keyboard_return.svg",
  "keys/keyboard_ctrl.svg",
  "keys/keyboard_ctrl_left.svg",
  "keys/keyboard_ctrl_right.svg",
  "keys/keyboard_alt.svg",
  "keys/keyboard_shift.svg",
  "keys/keyboard_tab.svg",
  "keys/keyboard_arrow_left.svg",
  "keys/keyboard_arrow_right.svg",
  "keys/keyboard_arrow_up.svg",
  "keys/keyboard_arrow_down.svg",
  "keys/keyboard_f1.svg",
  "keys/keyboard_f2.svg",
  "keys/keyboard_f3.svg",
  "keys/keyboard_f4.svg",
  "keys/keyboard_f5.svg",
  "keys/keyboard_f6.svg",
  "keys/keyboard_f7.svg",
  "keys/keyboard_f8.svg",
  "keys/keyboard_f9.svg",
  "keys/keyboard_f10.svg",
  "keys/keyboard_f11.svg",
  "keys/keyboard_f12.svg",
};

const char *vui_get_keyicon_from_scancode(int scancode) {
    int icon_index = -1;

    switch (scancode) {
        case SDL_SCANCODE_A: icon_index = VUI_KEYBOARD_ICON_A; break;
        case SDL_SCANCODE_B: icon_index = VUI_KEYBOARD_ICON_B; break;
        case SDL_SCANCODE_C: icon_index = VUI_KEYBOARD_ICON_C; break;
        case SDL_SCANCODE_D: icon_index = VUI_KEYBOARD_ICON_D; break;
        case SDL_SCANCODE_E: icon_index = VUI_KEYBOARD_ICON_E; break;
        case SDL_SCANCODE_F: icon_index = VUI_KEYBOARD_ICON_F; break;
        case SDL_SCANCODE_G: icon_index = VUI_KEYBOARD_ICON_G; break;
        case SDL_SCANCODE_H: icon_index = VUI_KEYBOARD_ICON_H; break;
        case SDL_SCANCODE_I: icon_index = VUI_KEYBOARD_ICON_I; break;
        case SDL_SCANCODE_J: icon_index = VUI_KEYBOARD_ICON_J; break;
        case SDL_SCANCODE_K: icon_index = VUI_KEYBOARD_ICON_K; break;
        case SDL_SCANCODE_L: icon_index = VUI_KEYBOARD_ICON_L; break;
        case SDL_SCANCODE_M: icon_index = VUI_KEYBOARD_ICON_M; break;
        case SDL_SCANCODE_N: icon_index = VUI_KEYBOARD_ICON_N; break;
        case SDL_SCANCODE_O: icon_index = VUI_KEYBOARD_ICON_O; break;
        case SDL_SCANCODE_P: icon_index = VUI_KEYBOARD_ICON_P; break;
        case SDL_SCANCODE_Q: icon_index = VUI_KEYBOARD_ICON_Q; break;
        case SDL_SCANCODE_R: icon_index = VUI_KEYBOARD_ICON_R; break;
        case SDL_SCANCODE_S: icon_index = VUI_KEYBOARD_ICON_S; break;
        case SDL_SCANCODE_T: icon_index = VUI_KEYBOARD_ICON_T; break;
        case SDL_SCANCODE_U: icon_index = VUI_KEYBOARD_ICON_U; break;
        case SDL_SCANCODE_V: icon_index = VUI_KEYBOARD_ICON_V; break;
        case SDL_SCANCODE_W: icon_index = VUI_KEYBOARD_ICON_W; break;
        case SDL_SCANCODE_X: icon_index = VUI_KEYBOARD_ICON_X; break;
        case SDL_SCANCODE_Y: icon_index = VUI_KEYBOARD_ICON_Y; break;
        case SDL_SCANCODE_Z: icon_index = VUI_KEYBOARD_ICON_Z; break;

        case SDL_SCANCODE_0: 
        case SDL_SCANCODE_KP_0: icon_index = VUI_KEYBOARD_ICON_0; break;
        case SDL_SCANCODE_1: 
        case SDL_SCANCODE_KP_1: icon_index = VUI_KEYBOARD_ICON_1; break;
        case SDL_SCANCODE_2: 
        case SDL_SCANCODE_KP_2: icon_index = VUI_KEYBOARD_ICON_2; break;
        case SDL_SCANCODE_3: 
        case SDL_SCANCODE_KP_3: icon_index = VUI_KEYBOARD_ICON_3; break;
        case SDL_SCANCODE_4: 
        case SDL_SCANCODE_KP_4: icon_index = VUI_KEYBOARD_ICON_4; break;
        case SDL_SCANCODE_5: 
        case SDL_SCANCODE_KP_5: icon_index = VUI_KEYBOARD_ICON_5; break;
        case SDL_SCANCODE_6: 
        case SDL_SCANCODE_KP_6: icon_index = VUI_KEYBOARD_ICON_6; break;
        case SDL_SCANCODE_7: 
        case SDL_SCANCODE_KP_7: icon_index = VUI_KEYBOARD_ICON_7; break;
        case SDL_SCANCODE_8: 
        case SDL_SCANCODE_KP_8: icon_index = VUI_KEYBOARD_ICON_8; break;
        case SDL_SCANCODE_9: 
        case SDL_SCANCODE_KP_9: icon_index = VUI_KEYBOARD_ICON_9; break;

        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_RETURN2:
        case SDL_SCANCODE_KP_ENTER: icon_index = VUI_KEYBOARD_ICON_RET; break;

        case SDL_SCANCODE_LCTRL:    icon_index = VUI_KEYBOARD_ICON_LCTRL; break;
        case SDL_SCANCODE_RCTRL:    icon_index = VUI_KEYBOARD_ICON_RCTRL; break;

        case SDL_SCANCODE_LALT:
        case SDL_SCANCODE_RALT:     icon_index = VUI_KEYBOARD_ICON_ALT; break;

        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT:   icon_index = VUI_KEYBOARD_ICON_SHIFT; break;

        case SDL_SCANCODE_TAB:      icon_index = VUI_KEYBOARD_ICON_TAB; break;

        case SDL_SCANCODE_LEFT:     icon_index = VUI_KEYBOARD_ICON_LARROW; break;
        case SDL_SCANCODE_RIGHT:    icon_index = VUI_KEYBOARD_ICON_RARROW; break;
        case SDL_SCANCODE_UP:       icon_index = VUI_KEYBOARD_ICON_UARROW; break;
        case SDL_SCANCODE_DOWN:     icon_index = VUI_KEYBOARD_ICON_DARROW; break;

        case SDL_SCANCODE_F1:       icon_index = VUI_KEYBOARD_ICON_F1; break;
        case SDL_SCANCODE_F2:       icon_index = VUI_KEYBOARD_ICON_F2; break;
        case SDL_SCANCODE_F3:       icon_index = VUI_KEYBOARD_ICON_F3; break;
        case SDL_SCANCODE_F4:       icon_index = VUI_KEYBOARD_ICON_F4; break;
        case SDL_SCANCODE_F5:       icon_index = VUI_KEYBOARD_ICON_F5; break;
        case SDL_SCANCODE_F6:       icon_index = VUI_KEYBOARD_ICON_F6; break;
        case SDL_SCANCODE_F7:       icon_index = VUI_KEYBOARD_ICON_F7; break;
        case SDL_SCANCODE_F8:       icon_index = VUI_KEYBOARD_ICON_F8; break;
        case SDL_SCANCODE_F9:       icon_index = VUI_KEYBOARD_ICON_F9; break;
        case SDL_SCANCODE_F10:      icon_index = VUI_KEYBOARD_ICON_F10; break;
        case SDL_SCANCODE_F11:      icon_index = VUI_KEYBOARD_ICON_F11; break;
        case SDL_SCANCODE_F12:      icon_index = VUI_KEYBOARD_ICON_F12; break;

        default:
            return NULL; 
    }

    if (icon_index >= 0 && icon_index < VUI_KEYBOARD_ICON_COUNT) {
        return VUI_KEYBOARD_ICONS[icon_index];
    }

    return NULL;
}