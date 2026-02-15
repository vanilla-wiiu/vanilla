#include "ui_sdl.h"

#include <stdatomic.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_hints.h>
#include <SDL2/SDL_syswm.h>
#include <SDL_image.h>
#include <SDL_power.h>
#include <SDL_ttf.h>
#include <pthread.h>
#include <vanilla.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_drm.h>
#include <unistd.h>

#ifdef VANILLA_HAS_EGL
#include <SDL2/SDL_egl.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengles2.h>
#endif

#ifdef VANILLA_DRM_AVAILABLE
#include <drm_fourcc.h>
#include "ui_sdl_drm.h"
#endif

#include "config.h"
#include "menu/menu.h"
#include "menu/menu_game.h"
#include "platform.h"
#include "ui_priv.h"
#include "ui_util.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define PW_CHAR_SIZE 20
#define PW_CHAR_PAD 2
#define AUDIO_BUFFER_SIZE 1664 // Wii U never deviates from this so we can hardcode it
#define AUDIO_BUFFER_COUNT 3

typedef struct {
    SDL_Texture *texture;
    int w;
    int h;
    char text[MAX_BUTTON_TEXT];
    char icon[MAX_BUTTON_TEXT];
    int32_t icon_mod;
    int checked;
} vui_sdl_cached_texture_t;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Rect dst_rect;
    SDL_Texture *background;
    SDL_Texture *layer_data[MAX_BUTTON_COUNT];
    SDL_Texture *label_data[MAX_BUTTON_COUNT];
    vui_sdl_cached_texture_t button_cache[MAX_BUTTON_COUNT];
    vui_sdl_cached_texture_t image_cache[MAX_BUTTON_COUNT];
    vui_sdl_cached_texture_t textedit_cache[MAX_BUTTON_COUNT];
    TTF_Font *sysfont;
    TTF_Font *sysfont_small;
    TTF_Font *sysfont_tiny;
    SDL_AudioDeviceID audio;
    SDL_AudioDeviceID mic;
    SDL_GameController *controller;
    SDL_GameController *controller_gyros;
    SDL_Texture *game_tex;
    int last_shown_toast;
    SDL_Texture *toast_tex;
    struct timeval toast_expiry;
    AVFrame *frame;
	SDL_Texture *pw_tex;

	uint8_t audio_buffer[AUDIO_BUFFER_COUNT][AUDIO_BUFFER_SIZE];

    _Atomic size_t audio_wseq;
    _Atomic size_t audio_rseq;

	Uint32 last_power_state_check;
	vui_power_state_t last_power_state;

	uint16_t last_vibration_state;
} vui_sdl_context_t;

static int vibrate = 0;

void init_gamepad(vui_context_t *ctx)
{
	vibrate = 0;

    // Set up default keys/buttons/axes
    ctx->default_button_map[SDL_CONTROLLER_BUTTON_A] = VANILLA_BTN_A;
    ctx->default_button_map[SDL_CONTROLLER_BUTTON_B] = VANILLA_BTN_B;
    ctx->default_button_map[SDL_CONTROLLER_BUTTON_X] = VANILLA_BTN_X;
    ctx->default_button_map[SDL_CONTROLLER_BUTTON_Y] = VANILLA_BTN_Y;
    ctx->default_button_map[SDL_CONTROLLER_BUTTON_BACK] = VANILLA_BTN_MINUS;
    ctx->default_button_map[SDL_CONTROLLER_BUTTON_GUIDE] = VANILLA_BTN_HOME;
    ctx->default_button_map[SDL_CONTROLLER_BUTTON_MISC1] = VANILLA_BTN_TV;
    ctx->default_button_map[SDL_CONTROLLER_BUTTON_START] = VANILLA_BTN_PLUS;
    ctx->default_button_map[SDL_CONTROLLER_BUTTON_LEFTSTICK] = VANILLA_BTN_L3;
    ctx->default_button_map[SDL_CONTROLLER_BUTTON_RIGHTSTICK] = VANILLA_BTN_R3;
    ctx->default_button_map[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] = VANILLA_BTN_L;
    ctx->default_button_map[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = VANILLA_BTN_R;
    ctx->default_button_map[SDL_CONTROLLER_BUTTON_DPAD_UP] = VANILLA_BTN_UP;
    ctx->default_button_map[SDL_CONTROLLER_BUTTON_DPAD_DOWN] = VANILLA_BTN_DOWN;
    ctx->default_button_map[SDL_CONTROLLER_BUTTON_DPAD_LEFT] = VANILLA_BTN_LEFT;
    ctx->default_button_map[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] = VANILLA_BTN_RIGHT;
    ctx->default_axis_map[SDL_CONTROLLER_AXIS_LEFTX] = VANILLA_AXIS_L_X;
    ctx->default_axis_map[SDL_CONTROLLER_AXIS_LEFTY] = VANILLA_AXIS_L_Y;
    ctx->default_axis_map[SDL_CONTROLLER_AXIS_RIGHTX] = VANILLA_AXIS_R_X;
    ctx->default_axis_map[SDL_CONTROLLER_AXIS_RIGHTY] = VANILLA_AXIS_R_Y;
    ctx->default_axis_map[SDL_CONTROLLER_AXIS_TRIGGERLEFT] = VANILLA_BTN_ZL;
    ctx->default_axis_map[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] = VANILLA_BTN_ZR;
    ctx->default_key_map[SDL_SCANCODE_UP] = VANILLA_BTN_UP;
    ctx->default_key_map[SDL_SCANCODE_DOWN] = VANILLA_BTN_DOWN;
    ctx->default_key_map[SDL_SCANCODE_LEFT] = VANILLA_BTN_LEFT;
    ctx->default_key_map[SDL_SCANCODE_RIGHT] = VANILLA_BTN_RIGHT;
    ctx->default_key_map[SDL_SCANCODE_Z] = VANILLA_BTN_A;
    ctx->default_key_map[SDL_SCANCODE_X] = VANILLA_BTN_B;
    ctx->default_key_map[SDL_SCANCODE_C] = VANILLA_BTN_X;
    ctx->default_key_map[SDL_SCANCODE_V] = VANILLA_BTN_Y;
    ctx->default_key_map[SDL_SCANCODE_RETURN] = VANILLA_BTN_PLUS;
    ctx->default_key_map[SDL_SCANCODE_LCTRL] = VANILLA_BTN_MINUS;
    ctx->default_key_map[SDL_SCANCODE_H] = VANILLA_BTN_HOME;
    ctx->default_key_map[SDL_SCANCODE_Y] = VANILLA_BTN_TV;
    ctx->default_key_map[SDL_SCANCODE_W] = VANILLA_AXIS_L_UP;
    ctx->default_key_map[SDL_SCANCODE_A] = VANILLA_AXIS_L_LEFT;
    ctx->default_key_map[SDL_SCANCODE_S] = VANILLA_AXIS_L_DOWN;
    ctx->default_key_map[SDL_SCANCODE_D] = VANILLA_AXIS_L_RIGHT;
    ctx->default_key_map[SDL_SCANCODE_E] = VANILLA_BTN_L3;
    ctx->default_key_map[SDL_SCANCODE_KP_8] = VANILLA_AXIS_R_UP;
    ctx->default_key_map[SDL_SCANCODE_KP_4] = VANILLA_AXIS_R_LEFT;
    ctx->default_key_map[SDL_SCANCODE_KP_2] = VANILLA_AXIS_R_DOWN;
    ctx->default_key_map[SDL_SCANCODE_KP_6] = VANILLA_AXIS_R_RIGHT;
    ctx->default_key_map[SDL_SCANCODE_KP_5] = VANILLA_BTN_R3;
    ctx->default_key_map[SDL_SCANCODE_T] = VANILLA_BTN_L;
    ctx->default_key_map[SDL_SCANCODE_G] = VANILLA_BTN_ZL;
    ctx->default_key_map[SDL_SCANCODE_U] = VANILLA_BTN_R;
    ctx->default_key_map[SDL_SCANCODE_J] = VANILLA_BTN_ZR;
    ctx->default_key_map[SDL_SCANCODE_F5] = VPI_ACTION_TOGGLE_RECORDING;
    ctx->default_key_map[SDL_SCANCODE_F12] = VPI_ACTION_SCREENSHOT;
    ctx->default_key_map[SDL_SCANCODE_ESCAPE] = VPI_ACTION_DISCONNECT;
}

void find_valid_controller(vui_sdl_context_t *sdl_ctx)
{
    // HACK: This is just for the Steam Deck. The Steam Deck provides a "virtual gamepad"
    //       that it expects games to use, however the "virtual gamepad" doesn't provide
    //       direct access to the Steam Deck's gyroscopes. We can bypass it, but then
    //       Steam Deck users lose usability features like remapping (and Vanilla will
    //       continue to receive inputs in the background, which are usually cut off on
    //       the virtual gamepad). To try to get the best of both worlds, we use a hybrid
    //       approach where we use the virtual gamepad for everything EXCEPT the gyros,
    //       and then use the direct hardware access JUST for the gyros.
    int controller = -1;
    int steam_virtual_gamepad_index = -1;

	vpilog("Looking for game controllers...\n");
	for (int i = 0; i < SDL_NumJoysticks(); i++) {
        const char *ctrl_name = SDL_GameControllerNameForIndex(i);
		vpilog("  Found %i: %s\n", i, ctrl_name);
        if (ctrl_name != NULL && !strcmp(ctrl_name, "Steam Virtual Gamepad")) {
            steam_virtual_gamepad_index = i;
        } else if (controller == -1) {
            controller = i;
        }
	}

    SDL_GameController *steam_virtual_gamepad = NULL;
    SDL_GameController *c = NULL;
    if (steam_virtual_gamepad_index != -1) {
        // Prefer the Steam Virtual Gamepad when present for everything EXCEPT gyros
        steam_virtual_gamepad = SDL_GameControllerOpen(steam_virtual_gamepad_index);
        vpilog("Using Steam Virtual Gamepad\n");
    }

    if (controller != -1) {
        // Open regular controller
        c = SDL_GameControllerOpen(controller);
        if (c) {
            // Enable gyro/accelerometer
            const char *ctrl_name = SDL_GameControllerName(c);
            SDL_GameControllerSetSensorEnabled(c, SDL_SENSOR_ACCEL, 1);
            SDL_GameControllerSetSensorEnabled(c, SDL_SENSOR_GYRO, 1);
            if (steam_virtual_gamepad) {
                vpilog("  with \"%s\" for accelerometer/gyroscope\n", ctrl_name);
            } else {
                vpilog("Using game controller \"%s\"\n", ctrl_name);
            }
        }
    }

    if (steam_virtual_gamepad) {
        sdl_ctx->controller = steam_virtual_gamepad;
        sdl_ctx->controller_gyros = c;
    } else {
        sdl_ctx->controller = c;
    }
}

void vui_sdl_audio_handler(const void *data, size_t size, void *userdata)
{
	vui_sdl_context_t *sdl_ctx = (vui_sdl_context_t *) userdata;

	if (!sdl_ctx->audio) {
		// Buffers will not have been initialized, so return before we try using them
		return;
	}

    // static Uint32 last_call = 0;
    // Uint32 now = SDL_GetTicks();
    // vanilla_log("** PRODUCING AT %u delta %u", now, now - last_call);
    // last_call = now;

    size_t w = atomic_load_explicit(&sdl_ctx->audio_wseq, memory_order_relaxed);
    uint8_t *buf = sdl_ctx->audio_buffer[w % AUDIO_BUFFER_COUNT];
    memcpy(buf, data, size);
    atomic_fetch_add_explicit(&sdl_ctx->audio_wseq, 1, memory_order_release);
}

void vui_sdl_vibrate_handler(uint8_t vibrate, void *userdata)
{
    vui_sdl_context_t *sdl_ctx = (vui_sdl_context_t *) userdata;
    if (sdl_ctx->controller) {
        uint16_t amount = vibrate ? 0xFFFF : 0;
		if (sdl_ctx->last_vibration_state != amount) {
			SDL_GameControllerRumble(sdl_ctx->controller, amount, amount, 0);
			sdl_ctx->last_vibration_state = amount;
		}
    }
}

TTF_Font *get_font(vui_sdl_context_t *sdl_ctx, vui_font_size_t size)
{
    switch (size) {
    case VUI_FONT_SIZE_TINY:
        return sdl_ctx->sysfont_tiny;
    case VUI_FONT_SIZE_SMALL:
        return sdl_ctx->sysfont_small;
    case VUI_FONT_SIZE_NORMAL:
    default:
        return sdl_ctx->sysfont;
    }
}

int vui_sdl_font_height_handler(vui_font_size_t size, void *userdata)
{
    vui_sdl_context_t *sdl_ctx = (vui_sdl_context_t *) userdata;
    TTF_Font *font = get_font(sdl_ctx, size);
    int h = 0;
    if (font) {
        TTF_SizeText(font, "Hj", 0, &h);
    }
    return h;
}

void vui_sdl_text_open_handler(vui_context_t *ctx, int textedit, int open, void *userdata)
{
    if (open) {
        vui_textedit_t *edit = &ctx->textedits[textedit];
        SDL_Rect r;
        r.x = edit->x;
        r.y = edit->y;
        r.w = edit->w;
        r.h = edit->h;
        SDL_SetTextInputRect(&r);
        SDL_StartTextInput();
    } else {
        SDL_StopTextInput();
    }
}

vui_power_state_t vui_sdl_power_state_handler(vui_context_t *ctx, int *percent)
{
	vui_sdl_context_t *sdl_ctx = (vui_sdl_context_t *) ctx->platform_data;

	// Turns out checking the power state is an expensive operation. Let's
	// limit checks to once per minute and cache the result.
	Uint32 now = SDL_GetTicks();
	if (now >= sdl_ctx->last_power_state_check + 60000) {
		sdl_ctx->last_power_state = (vui_power_state_t) SDL_GetPowerInfo(NULL, percent);
		sdl_ctx->last_power_state_check = now;
	}

	return sdl_ctx->last_power_state;
}

void vui_sdl_fullscreen_enabled_handler(vui_context_t *ctx, int enabled, void *userdata)
{
    vui_sdl_context_t *sdl_ctx = (vui_sdl_context_t *) ctx->platform_data;

    SDL_SetWindowFullscreen(sdl_ctx->window, enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    SDL_ShowCursor(!enabled);
}

void vui_sdl_get_key_mapping_handler(vui_context_t *ctx, int vanilla_btn, void *userdata)
{

}

void vui_sdl_set_key_mapping_handler(vui_context_t *ctx, int vanilla_btn, int key, void *userdata)
{

}

void vui_sdl_audio_set_enabled(vui_context_t *ctx, int enabled, void *userdata)
{
    vui_sdl_context_t *sdl_ctx = (vui_sdl_context_t *) userdata;

    SDL_PauseAudioDevice(sdl_ctx->audio, !enabled);
}

void vui_sdl_mic_set_enabled(vui_context_t *ctx, int enabled, void *userdata)
{
    vui_sdl_context_t *sdl_ctx = (vui_sdl_context_t *) userdata;

	SDL_PauseAudioDevice(sdl_ctx->mic, !enabled);
}

void audio_callback(void *userdata, Uint8 *stream, int len)
{
	vui_sdl_context_t *sdl_ctx = (vui_sdl_context_t *) userdata;

    // static Uint32 last_call = 0;
    // Uint32 now = SDL_GetTicks();
    // vanilla_log("== CONSUMING AT %u delta %u", now, now - last_call);
    // last_call = now;

    size_t r = atomic_load_explicit(&sdl_ctx->audio_rseq, memory_order_relaxed);
    size_t w = atomic_load_explicit(&sdl_ctx->audio_wseq, memory_order_acquire);
    if (r != w) {
        if (r < w - AUDIO_BUFFER_COUNT + 1) {
            size_t newr = w - AUDIO_BUFFER_COUNT + 1;;
            vanilla_log("  AUDIO SKIPPED FROM %zu TO %zu", r, newr);
            r = newr;
        }
        memcpy(stream, sdl_ctx->audio_buffer[r % AUDIO_BUFFER_COUNT], len);
        r++;
        atomic_store_explicit(&sdl_ctx->audio_rseq, r, memory_order_release);
    } else {
        memset(stream, 0, len);
    }
}

void mic_callback(void *userdata, Uint8 *stream, int len)
{
	vui_context_t *ctx = (vui_context_t *) userdata;
	if (ctx->mic_callback)
    	ctx->mic_callback(ctx->mic_callback_data, stream, len);
}

int32_t pack_float(float f)
{
    int32_t x;
    memcpy(&x, &f, sizeof(int32_t));
    return x;
}

int vui_sdl_event_thread(void *data)
{
    vui_context_t *vui = (vui_context_t *) data;
    vui_sdl_context_t *sdl_ctx = (vui_sdl_context_t *) vui->platform_data;

    SDL_Event ev;
    // while (!vui->quit) {
        // while (SDL_WaitEventTimeout(&ev, 100)) {
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT:
                vanilla_stop();
                vui_quit(vui);
                return 0;
            case SDL_MOUSEMOTION:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            {
                // Ensure dst_rect is initialized
                SDL_Rect *dst_rect = &sdl_ctx->dst_rect;
                if (dst_rect->w > 0 && dst_rect->h > 0) {
                    // Translate screen coords to logical coords
                    int tr_x, tr_y;
                    tr_x = (ev.button.x - dst_rect->x) * vui->screen_width / dst_rect->w;
                    tr_y = (ev.button.y - dst_rect->y) * vui->screen_height / dst_rect->h;

                    if (vui->game_mode) {
                        // In game mode, pass clicks directly to Vanilla
                        int x, y;
                        if (ev.button.button == SDL_BUTTON_LEFT && (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEMOTION)) {
                            x = tr_x;
                            y = tr_y;
                        } else {
                            x = -1;
                            y = -1;
                        }
                        vanilla_set_touch(x, y);
                    } else {
                        // Otherwise, handle ourselves
                        if (ev.type == SDL_MOUSEBUTTONDOWN)
                            vui_process_mousedown(vui, tr_x, tr_y);
                        else if (ev.type == SDL_MOUSEBUTTONUP)
                            vui_process_mouseup(vui, tr_x, tr_y);

                        if (vui->active_textedit != -1 && (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEMOTION) && ev.button.button == SDL_BUTTON_LEFT) {
                            // Put cursor in correct position
                            vui_textedit_t *edit = &vui->textedits[vui->active_textedit];
                            TTF_Font *font = get_font(sdl_ctx, edit->size);

                            // Determine best location for new cursor
                            int rel_x = tr_x - edit->x;
                            char tmp[MAX_BUTTON_TEXT];
                            int new_cursor = 0;
                            int diff = rel_x;
                            size_t len = 0;
                            char *src = edit->text;
                            while (*src != 0) {
                                src = vui_utf8_advance(src);

                                size_t len = src - edit->text;
                                strncpy(tmp, edit->text, len);
                                tmp[len] = 0;

                                int tw;
                                TTF_SizeUTF8(font, tmp, &tw, 0);

                                int this_diff = abs(tw - rel_x);
                                if (this_diff < diff) {
                                    diff = this_diff;
                                    new_cursor++;
                                } else {
                                    break;
                                }
                            }

                            vui_textedit_set_cursor(vui, vui->active_textedit, new_cursor);
                        }
                    }
                }
                break;
            }
            case SDL_CONTROLLERDEVICEADDED:
                // Attempt to find controller if one doesn't exist already
                if (!sdl_ctx->controller) {
                    find_valid_controller(sdl_ctx);
                }
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                // Handle current controller being removed
                if ((sdl_ctx->controller && ev.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(sdl_ctx->controller)))
                    || (sdl_ctx->controller_gyros && ev.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(sdl_ctx->controller_gyros)))) {
                    if (sdl_ctx->controller) {
                        SDL_GameControllerClose(sdl_ctx->controller);
                        sdl_ctx->controller = NULL;
                    }
                    if (sdl_ctx->controller_gyros) {
                        SDL_GameControllerClose(sdl_ctx->controller_gyros);
                        sdl_ctx->controller_gyros = NULL;
                    }
                    find_valid_controller(sdl_ctx);
                }
                break;
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
                if (sdl_ctx->controller && ev.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(sdl_ctx->controller))) {
                    int btn_idx = ev.cbutton.button;
                    int vanilla_btn;

                    // First try to read from config. If unmapped, fallback to default.
                    vanilla_btn = vpi_config.buttonmap[btn_idx];
                    if (vanilla_btn == VPI_CONFIG_UNMAPPED) {
                        vanilla_btn = vui->default_button_map[btn_idx];
                    }

                    if (vanilla_btn != -1) {
                        if (vanilla_btn > VPI_ACTION_START_INDEX) {
                            if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                                vpi_menu_action(vui, (vpi_extra_action_t) vanilla_btn);
                            }
                        } else {
                            // Handle ABXY swap
                            if (vpi_config.swap_abxy) {
                                switch (vanilla_btn) {
                                case VANILLA_BTN_A: vanilla_btn = VANILLA_BTN_B; break;
                                case VANILLA_BTN_B: vanilla_btn = VANILLA_BTN_A; break;
                                case VANILLA_BTN_X: vanilla_btn = VANILLA_BTN_Y; break;
                                case VANILLA_BTN_Y: vanilla_btn = VANILLA_BTN_X; break;
                                }
                            }

                            if (vui->game_mode) {
                                vanilla_set_button(vanilla_btn, ev.type == SDL_CONTROLLERBUTTONDOWN ? INT16_MAX : 0);
                            } else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                                vui_process_keydown(vui, vanilla_btn);
                            } else {
                                vui_process_keyup(vui, vanilla_btn);
                            }
                        }
                    }
                }
                break;
            case SDL_CONTROLLERAXISMOTION:
                if (sdl_ctx->controller && ev.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(sdl_ctx->controller))) {
                    int axis_idx = ev.caxis.axis;
                    int vanilla_axis;

                    // First try to read from config. If unmapped, fallback to default.
                    vanilla_axis = vpi_config.axismap[axis_idx];
                    if (vanilla_axis == VPI_CONFIG_UNMAPPED) {
                        vanilla_axis = vui->default_axis_map[ev.caxis.axis];
                    }

                    Sint16 axis_value = ev.caxis.value;
                    if (vanilla_axis != -1) {
                        if (vui->game_mode) {
                            vanilla_set_button(vanilla_axis, axis_value);
                        } else if (vanilla_axis == SDL_CONTROLLER_AXIS_LEFTX) {
                            if (axis_value < 0)
                                vui_process_keydown(vui, VANILLA_AXIS_L_LEFT);
                            else if (axis_value > 0)
                                vui_process_keydown(vui, VANILLA_AXIS_L_RIGHT);
                            else {
                                vui_process_keyup(vui, VANILLA_AXIS_L_LEFT);
                                vui_process_keyup(vui, VANILLA_AXIS_L_RIGHT);
                            }
                        } else if (vanilla_axis == SDL_CONTROLLER_AXIS_LEFTY) {
                            if (axis_value < 0)
                                vui_process_keydown(vui, VANILLA_AXIS_L_UP);
                            else if (axis_value > 0)
                                vui_process_keydown(vui, VANILLA_AXIS_L_DOWN);
                            else {
                                vui_process_keyup(vui, VANILLA_AXIS_L_UP);
                                vui_process_keyup(vui, VANILLA_AXIS_L_DOWN);
                            }
                        }
                    }
                }
                break;
            case SDL_CONTROLLERSENSORUPDATE:
                if (ev.csensor.sensor == SDL_SENSOR_ACCEL) {
                    vanilla_set_button(VANILLA_SENSOR_ACCEL_X, pack_float(ev.csensor.data[0]));
                    vanilla_set_button(VANILLA_SENSOR_ACCEL_Y, pack_float(ev.csensor.data[1]));
                    vanilla_set_button(VANILLA_SENSOR_ACCEL_Z, pack_float(ev.csensor.data[2]));
                } else if (ev.csensor.sensor == SDL_SENSOR_GYRO) {
                    vanilla_set_button(VANILLA_SENSOR_GYRO_PITCH, pack_float(ev.csensor.data[0]));
                    vanilla_set_button(VANILLA_SENSOR_GYRO_YAW, pack_float(ev.csensor.data[1]));
                    vanilla_set_button(VANILLA_SENSOR_GYRO_ROLL, pack_float(ev.csensor.data[2]));
                }
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                if (vui->key_override_handler) {
                    if (ev.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                        vui->key_override_cancel_handler(vui, vui->key_override_handler_data);
                    } else {
                        vui->key_override_handler(vui, ev.key.keysym.scancode, vui->key_override_handler_data);
                    }
                } else if (vui->active_textedit == -1) {
                    int key_idx = ev.key.keysym.scancode;
                    int vanilla_btn;

                    // First try to read from config. If unmapped, fallback to default.
                    vanilla_btn = vpi_config.keymap[key_idx];
                    if (vanilla_btn == VPI_CONFIG_UNMAPPED) {
                        vanilla_btn = vui->default_key_map[key_idx];
                    }

                    if (vanilla_btn > VPI_ACTION_START_INDEX) {
                        if (ev.type == SDL_KEYDOWN)
                            vpi_menu_action(vui, (vpi_extra_action_t) vanilla_btn);
                    } else if (vanilla_btn != -1) {
                        if (vui->game_mode) {
                            vanilla_set_button(vanilla_btn, ev.type == SDL_KEYDOWN ? INT16_MAX : 0);
                        } else if (ev.type == SDL_KEYDOWN) {
                            vui_process_keydown(vui, vanilla_btn);
                        } else {
                            vui_process_keyup(vui, vanilla_btn);
                        }
                    }
                } else if (ev.type == SDL_KEYDOWN) {
                    switch (ev.key.keysym.scancode) {
                    case SDL_SCANCODE_BACKSPACE:
                        vui_textedit_backspace(vui, vui->active_textedit);
                        break;
                    case SDL_SCANCODE_LEFT:
                        vui_textedit_move_cursor(vui, vui->active_textedit, -1);
                        break;
                    case SDL_SCANCODE_RIGHT:
                        vui_textedit_move_cursor(vui, vui->active_textedit, 1);
                        break;
                    case SDL_SCANCODE_HOME:
                        vui_textedit_move_cursor(vui, vui->active_textedit, -MAX_BUTTON_TEXT);
                        break;
                    case SDL_SCANCODE_END:
                        vui_textedit_move_cursor(vui, vui->active_textedit, MAX_BUTTON_TEXT);
                        break;
                    case SDL_SCANCODE_DELETE:
                        vui_textedit_del(vui, vui->active_textedit);
                        break;
                    }
                }
                break;
            }
            case SDL_TEXTINPUT:
                vui_textedit_input(vui, vui->active_textedit, ev.text.text);
                break;
            case SDL_TEXTEDITING:
                vpilog("text editing!\n");
                break;
            }
        }
    // }

    return 0;
}

int vui_init_sdl(vui_context_t *ctx, int fullscreen)
{
	// Enable Steam Deck gyroscopes even while Steam is open and in gaming mode
	// SDL_SetHintWithPriority("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD", "0", SDL_HINT_OVERRIDE);
	SDL_SetHintWithPriority(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES, "", SDL_HINT_OVERRIDE);

	// One-time setup for SDL NV12
	SDL_SetHint("SDL_RENDER_OPENGL_NV12_RG_SHADER", "1");

	// Force EGL when using X11 so that VAAPI works
	SDL_SetHint(SDL_HINT_VIDEO_X11_FORCE_EGL, "1");

    // Enable linear filtering
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

	// By default, SDL2 uses X11, even on Wayland systems. This probably isn't
	// a big deal, but just in case we want it to prefer Wayland, this is how
	// we can do it.
	// SDL_SetHint(SDL_HINT_VIDEODRIVER, "wayland,x11");

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        vpilog("Failed to initialize SDL: %s\n", SDL_GetError());
        return -1;
    }

    vui_sdl_context_t *sdl_ctx = malloc(sizeof(vui_sdl_context_t));
    memset(sdl_ctx, 0, sizeof(vui_sdl_context_t));
    ctx->platform_data = sdl_ctx;

    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE;
    if (fullscreen) {
        window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        SDL_ShowCursor(0);
    }

    sdl_ctx->window = SDL_CreateWindow("Vanilla", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ctx->screen_width, ctx->screen_height, window_flags);
    if (!sdl_ctx->window) {
        vpilog("Failed to CreateWindow: %s\n", SDL_GetError());
        return -1;
    }

    sdl_ctx->renderer = SDL_CreateRenderer(sdl_ctx->window, -1, SDL_RENDERER_ACCELERATED);
    if (!sdl_ctx->renderer) {
        // Re-attempt without EGL (X11/NVIDIA do not support this)
	    SDL_SetHint(SDL_HINT_VIDEO_X11_FORCE_EGL, "0");
        vpi_egl_available = 0;

        sdl_ctx->renderer = SDL_CreateRenderer(sdl_ctx->window, -1, SDL_RENDERER_ACCELERATED);
        if (!sdl_ctx->renderer) {
            // It's not because we forced EGL, so it's something else we're not prepared for
            vpilog("Failed to CreateRenderer: %s\n", SDL_GetError());
            return -1;
        }
    }

    // Open audio output device
    SDL_AudioSpec desired = {0}, obtained;
    desired.freq = 48000;
    desired.format = AUDIO_S16LSB;
    desired.channels = 2;
	desired.callback = audio_callback;
	desired.userdata = sdl_ctx;
    desired.samples = AUDIO_BUFFER_SIZE / 2 / 2; // Buffer size (bytes) divided by 2 channels (stereo) divided by 2 bytes per sample (16-bit)

    sdl_ctx->audio = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (sdl_ctx->audio) {
        // vpilog("obtained.samples = %u\n", obtained.samples);

		// Set up buffers
        atomic_store(&sdl_ctx->audio_wseq, 0);
        atomic_store(&sdl_ctx->audio_rseq, 0);

		// Set up handler for audio submitted from VPI to VUI
        ctx->audio_handler = vui_sdl_audio_handler;
        ctx->audio_handler_data = sdl_ctx;

		// Set up handler for enabling audio
		ctx->audio_enabled_handler = vui_sdl_audio_set_enabled;
		ctx->audio_enabled_handler_data = sdl_ctx;
    } else {
        vpilog("Failed to open audio device\n");
    }

    // Open audio input device
    SDL_AudioSpec mic_desired = {0};
    mic_desired.freq = 16000;
    mic_desired.format = AUDIO_S16LSB;
    mic_desired.callback = mic_callback;
	mic_desired.userdata = ctx;
    mic_desired.channels = 1;
    sdl_ctx->mic = SDL_OpenAudioDevice(NULL, 1, &mic_desired, NULL, 0);
    if (sdl_ctx->mic) {
		ctx->mic_enabled_handler = vui_sdl_mic_set_enabled;
		ctx->mic_enabled_handler_data = sdl_ctx;
	} else {
		vpilog("Failed to open microphone device\n");
	}

    ctx->vibrate_handler = vui_sdl_vibrate_handler;
    ctx->vibrate_handler_data = sdl_ctx;
	sdl_ctx->last_vibration_state = 0;

    ctx->font_height_handler = vui_sdl_font_height_handler;
    ctx->font_height_handler_data = sdl_ctx;

    ctx->text_open_handler = vui_sdl_text_open_handler;

    ctx->power_state_handler = vui_sdl_power_state_handler;
	sdl_ctx->last_power_state_check = 0;
	sdl_ctx->last_power_state = VUI_POWERSTATE_UNKNOWN;

    ctx->fullscreen_enabled_handler = vui_sdl_fullscreen_enabled_handler;

    if (TTF_Init()) {
        vpilog("Failed to TTF_Init\n");
        return -1;
    }

    char font_filename[4096];
    vpi_asset_filename(font_filename, sizeof(font_filename), "font", "system.ttf");

    sdl_ctx->sysfont = TTF_OpenFont(font_filename, 36);
    if (!sdl_ctx->sysfont) {
        vpilog("Failed to TTF_OpenFont: %s\n", font_filename);
        return -1;
    }

    sdl_ctx->sysfont_small = TTF_OpenFont(font_filename, 24);
    if (!sdl_ctx->sysfont_small) {
        vpilog("Failed to TTF_OpenFont: %s\n", font_filename);
        return -1;
    }

    sdl_ctx->sysfont_tiny = TTF_OpenFont(font_filename, 16);
    if (!sdl_ctx->sysfont_tiny) {
        vpilog("Failed to TTF_OpenFont: %s\n", font_filename);
        return -1;
    }

    TTF_SetFontWrappedAlign(sdl_ctx->sysfont, TTF_WRAPPED_ALIGN_CENTER);
    TTF_SetFontWrappedAlign(sdl_ctx->sysfont_small, TTF_WRAPPED_ALIGN_CENTER);
    TTF_SetFontWrappedAlign(sdl_ctx->sysfont_tiny, TTF_WRAPPED_ALIGN_CENTER);

	sdl_ctx->game_tex = 0;

    sdl_ctx->background = 0;
    sdl_ctx->last_shown_toast = 0;
    sdl_ctx->toast_tex = 0;
	sdl_ctx->pw_tex = 0;
    memset(sdl_ctx->layer_data, 0, sizeof(sdl_ctx->layer_data));
    memset(sdl_ctx->label_data, 0, sizeof(sdl_ctx->label_data));
    memset(sdl_ctx->button_cache, 0, sizeof(sdl_ctx->button_cache));
    memset(sdl_ctx->image_cache, 0, sizeof(sdl_ctx->image_cache));
    memset(sdl_ctx->textedit_cache, 0, sizeof(sdl_ctx->textedit_cache));

    sdl_ctx->controller = NULL;
    sdl_ctx->controller_gyros = NULL;
    find_valid_controller(sdl_ctx);

    sdl_ctx->frame = av_frame_alloc();

	// Initialize gamepad lookup tables
	init_gamepad(ctx);

    return 0;
}

void vui_close_sdl(vui_context_t *ctx)
{
    vui_sdl_context_t *sdl_ctx = (vui_sdl_context_t *) ctx->platform_data;
    if (!sdl_ctx) {
        return;
    }

    av_frame_free(&sdl_ctx->frame);

    if (sdl_ctx->controller) {
        SDL_GameControllerClose(sdl_ctx->controller);
    }

	if (sdl_ctx->audio) {
		SDL_CloseAudioDevice(sdl_ctx->audio);
	}

	if (sdl_ctx->mic) {
		SDL_CloseAudioDevice(sdl_ctx->mic);
	}

	if (sdl_ctx->pw_tex) {
		SDL_DestroyTexture(sdl_ctx->pw_tex);
		sdl_ctx->pw_tex = 0;
	}

    SDL_DestroyTexture(sdl_ctx->toast_tex);
    if (sdl_ctx->game_tex) SDL_DestroyTexture(sdl_ctx->game_tex);
    SDL_DestroyTexture(sdl_ctx->background);

    for (int i = 0; i < MAX_BUTTON_COUNT; i++) {
        if (sdl_ctx->layer_data[i]) {
            SDL_DestroyTexture(sdl_ctx->layer_data[i]);
        }
        if (sdl_ctx->label_data[i]) {
            SDL_DestroyTexture(sdl_ctx->label_data[i]);
        }
        if (sdl_ctx->button_cache[i].texture) {
            SDL_DestroyTexture(sdl_ctx->button_cache[i].texture);
        }
        if (sdl_ctx->image_cache[i].texture) {
            SDL_DestroyTexture(sdl_ctx->image_cache[i].texture);
        }
        if (sdl_ctx->textedit_cache[i].texture) {
            SDL_DestroyTexture(sdl_ctx->textedit_cache[i].texture);
        }
    }

    TTF_CloseFont(sdl_ctx->sysfont_tiny);
    TTF_CloseFont(sdl_ctx->sysfont_small);
    TTF_CloseFont(sdl_ctx->sysfont);

    TTF_Quit();

	SDL_CloseAudioDevice(sdl_ctx->audio);

    SDL_DestroyRenderer(sdl_ctx->renderer);

    SDL_DestroyWindow(sdl_ctx->window);

    free(sdl_ctx);

    SDL_Quit();
}

float lerp(float a, float b, float f)
{
    return a * (1.0 - f) + (b * f);
}

void vui_sdl_set_render_color(SDL_Renderer *renderer, vui_color_t color)
{
    SDL_SetRenderDrawColor(
        renderer,
        color.r * 0xFF,
        color.g * 0xFF,
        color.b * 0xFF,
        color.a * 0xFF
    );
}

SDL_Texture *vui_sdl_load_texture(SDL_Renderer *renderer, const char *filename)
{
    char buf[1024];
    vpi_asset_filename(buf, sizeof(buf), "tex", filename);
    return IMG_LoadTexture(renderer, buf);
}

float calculate_x_inset(float y, float h, float radius)
{
    float x_inset = 0;
    if (y < radius) {
        float yb = y - radius;
        x_inset = radius - sqrtf(radius*radius - yb*yb);
    } else {
        if (y >= h - radius) {
            float yb = (h - y) - radius;
            x_inset = radius - sqrtf(radius*radius - yb*yb);
        }
    }
    return x_inset;
}

void determine_color_for_gradient(int y, int h, uint8_t *r, uint8_t *g, uint8_t *b, const float *position, const uint8_t *colors, size_t count)
{
    float yf = y / (float) h;
    for (size_t j = 1; j < count; j++) {
        float min = position[j - 1];
        float max = position[j];
        if (yf >= min && yf < max) {
            yf -= min;
            yf /= max - min;

            const uint8_t *minrgb = colors + (j - 1) * 3;
            const uint8_t *maxrgb = colors + j * 3;
            *r = lerp(minrgb[0], maxrgb[0], yf);
            *g = lerp(minrgb[1], maxrgb[1], yf);
            *b = lerp(minrgb[2], maxrgb[2], yf);
            break;
        }
    }
}

void vui_sdl_draw_button(vui_context_t *vui, vui_sdl_context_t *sdl_ctx, vui_button_t *btn, int msaa)
{
    SDL_SetRenderDrawColor(sdl_ctx->renderer, 0, 0, 0, 0);
    SDL_RenderClear(sdl_ctx->renderer);

    SDL_Color text_color;

    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = btn->w * msaa;
    rect.h = btn->h * msaa;

    // static const float button_x_gradient_positions[] = {
    //     0.0f,
    //     0.1f,
    //     0.9f,
    //     1.0f,
    // };
    // static const uint8_t button_x_gradient_colors[] = {
    //     0xE8,
    //     0xFF,
    //     0xFF,
    //     0xE8,
    // };
    static const float button_y_gradient_positions[] = {
        0.0f,
        0.1f,
        0.66f,
        0.875f,
        1.0f
    };
    static const uint8_t button_y_gradient_colors[] = {
        0xE8, 0xE8, 0xE8,
        0xFF, 0xFF, 0xFF,
        0xF8, 0xF8, 0xF8,
        0xE0, 0xE0, 0xE0,
        0xF0, 0xF0, 0xF0,
    };
    static const uint8_t button_checkable_y_gradient_colors[] = {
        0xCA, 0xCA, 0xCA,
        0xE5, 0xE5, 0xE5,
        0xE4, 0xE4, 0xE4,
        0xD6, 0xD6, 0xD6,
        0xD2, 0xD2, 0xD2,
    };
    static const uint8_t button_checked_y_gradient_colors[] = {
        0x50, 0xC4, 0x25,
        0x71, 0xE5, 0x28,
        0x7D, 0xE3, 0x06,
        0x4D, 0xCF, 0x00,
        0x64, 0xCD, 0x3D,
    };

    int scrw, scrh;
    vui_get_screen_size(vui, &scrw, &scrh);

    // Load icon if exists
    int icon_x;
    int icon_y;
    int icon_size = btn->style == VUI_BUTTON_STYLE_CORNER ? rect.h * 1 / 3 : btn->font_size == VUI_FONT_SIZE_SMALL ? rect.h : rect.h * 2 / 4;
    SDL_Texture *icon = 0;
    if (btn->icon[0]) {
        icon = vui_sdl_load_texture(sdl_ctx->renderer, btn->icon);
    }

    TTF_Font *text_font;
    if (btn->style == VUI_BUTTON_STYLE_CORNER) {
        if (!icon) {
            text_font = sdl_ctx->sysfont_small;
        } else {
            text_font = sdl_ctx->sysfont_tiny;
        }
    } else {
        switch (btn->font_size) {
        case VUI_FONT_SIZE_SMALL:
            text_font = sdl_ctx->sysfont_small;
            break;
        case VUI_FONT_SIZE_TINY:
            text_font = sdl_ctx->sysfont_tiny;
            break;
        default:
            text_font = sdl_ctx->sysfont;
            break;
        }
    }

    const int btn_radius = rect.h * 2 / 10;
    for (int y = 0; y < rect.h; y++) {
        int coord = rect.y + y;

        // Determine color
        uint8_t lr, lg, lb;
        determine_color_for_gradient(
            y, rect.h, &lr, &lg, &lb,
            button_y_gradient_positions,
            button_y_gradient_colors,
            sizeof(button_y_gradient_positions)/sizeof(float)
        );

        // Handle rounded corners
        float x_inset = 0;
        float w_inset = 0;
        if (btn->style != VUI_BUTTON_STYLE_CORNER) {
            x_inset = w_inset = calculate_x_inset(y, rect.h, btn_radius);
        } else {
            float inset_y = y;
            if (btn->y < scrh/2) {
                inset_y += rect.h;
            }

            float hor_inset = calculate_x_inset(inset_y, rect.h*2, rect.h);
            if (btn->x < scrw/2) {
                w_inset = hor_inset;
            } else {
                x_inset = hor_inset;
            }
        }

        // Make a cutoff section for the highlighted area
        if (btn->checkable) {
            uint8_t clr, clg, clb;
            int lim = btn_radius + btn_radius;

            determine_color_for_gradient(
                y, rect.h, &clr, &clg, &clb,
                button_y_gradient_positions,
                btn->checked ? button_checked_y_gradient_colors : button_checkable_y_gradient_colors,
                sizeof(button_y_gradient_positions)/sizeof(float)
            );

            SDL_SetRenderDrawColor(sdl_ctx->renderer, clr, clg, clb, 0xFF);
            SDL_RenderDrawLineF(sdl_ctx->renderer, rect.x + x_inset, coord, rect.x + lim - 1, coord);

            x_inset = lim;
        }

        SDL_SetRenderDrawColor(sdl_ctx->renderer, lr, lg, lb, 0xFF);
        SDL_RenderDrawLineF(sdl_ctx->renderer, rect.x + x_inset, coord, rect.x + rect.w - w_inset, coord);
    }

    text_color.r = text_color.g = text_color.b = 0x40;
    text_color.a = 0xFF;

    // Draw text
    if (btn->text[0]) {
        SDL_Surface *surface = TTF_RenderUTF8_Blended(text_font, btn->text, text_color);
        SDL_Texture *texture = SDL_CreateTextureFromSurface(sdl_ctx->renderer, surface);

        SDL_Rect text_rect;

        const int msaa_scaled_text_w = surface->w * msaa;
        const int msaa_scaled_text_h = surface->h * msaa;

        text_rect.w = msaa_scaled_text_w;
        text_rect.h = msaa_scaled_text_h;

        const int btn_padding = 8;
        text_rect.x = rect.x + rect.w / 2 - msaa_scaled_text_w / 2;
        text_rect.y = rect.y + rect.h / 2 - msaa_scaled_text_h / 2;

        // Adjust text rect if icon exists
        if (icon) {
            if (btn->style == VUI_BUTTON_STYLE_CORNER) {
                text_rect.y = rect.y + rect.h/2 - (msaa_scaled_text_h + icon_size + btn_padding)/2;
                icon_x = rect.x + rect.w/2 - icon_size/2;
                icon_y = text_rect.y + msaa_scaled_text_h + btn_padding;
            } else {
                const int total_width = icon_size + btn_padding + msaa_scaled_text_w;
                icon_x = rect.x + rect.w/2 - total_width/2;
                icon_y = rect.y + rect.h/2 - icon_size/2;
                text_rect.x = icon_x + icon_size + btn_padding;
            }
        }
        else {
            if (btn->style == VUI_BUTTON_STYLE_CORNER) {
                // we apply the formula of the circle quarter centroid
                // https://www.efunda.com/math/areas/CircleQuarter.cfm
                int scrw, scrh;
                vui_get_screen_size(vui, &scrw, &scrh);
                float fract = 4.0f / (3.0f * M_PI);
                float off_h = fract * rect.h;
                float off_w = fract * rect.w;
                // we treat the x and y offsets differently
                // to account for non-circular button shapes
                // we also verify which corner on the screen the button is placed
                // in order to adjust the offset
                float cx = rect.x + (btn->x < scrw / 2 ? off_w : (rect.w - off_w));
                float cy = rect.y + (btn->y < scrh / 2 ? off_h : (rect.h - off_h));

                text_rect.x = (int)(cx - msaa_scaled_text_w * 0.5f);
                text_rect.y = (int)(cy - msaa_scaled_text_h * 0.5f);
            }
        }

        SDL_RenderCopy(sdl_ctx->renderer, texture, NULL, &text_rect);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
    } else {
        icon_x = rect.x + rect.w/2 - icon_size/2;
        icon_y = rect.y + rect.h/2 - icon_size/2;
    }

    // Draw icon
    if (icon) {
        SDL_Rect icon_rect;
        icon_rect.w = icon_rect.h = icon_size;
        icon_rect.x = icon_x;
        icon_rect.y = icon_y;

        if(btn->icon_mod){
            char r = (btn->icon_mod & 0xFF0000) >> sizeof(char) * 8 * 2;
            char g = (btn->icon_mod & 0x00FF00) >> sizeof(char) * 8;
            char b = (btn->icon_mod & 0x0000FF);
            SDL_SetTextureColorMod(icon, r, g, b);
        }

        SDL_RenderCopy(sdl_ctx->renderer, icon, 0, &icon_rect);
        SDL_DestroyTexture(icon);
    }
}

void vui_sdl_draw_background(vui_context_t *ctx, SDL_Renderer *renderer)
{
    const int bg_r1 = 0xB8;
    const int bg_g1 = 0xB8;
    const int bg_b1 = 0xC8;
    const int bg_r2 = 0xE0;
    const int bg_g2 = 0xF0;
    const int bg_b2 = 0xF0;
    for (int x = 0; x < ctx->screen_width; x++) {
        float xf = x / (float) ctx->screen_width;
        SDL_SetRenderDrawColor(renderer, lerp(bg_r1, bg_r2, xf), lerp(bg_g1, bg_g2, xf), lerp(bg_b1, bg_b2, xf), 0xFF);
        SDL_RenderDrawLine(renderer, x, 0, x, ctx->screen_height);
    }
}

void vui_draw_sdl(vui_context_t *ctx, SDL_Renderer *renderer)
{
    vui_sdl_context_t *sdl_ctx = (vui_sdl_context_t *) ctx->platform_data;

    for (int layer = 0; layer < ctx->layers; layer++) {
        if (!sdl_ctx->layer_data[layer]) {
            // Create a new layer here
            sdl_ctx->layer_data[layer] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, ctx->screen_width, ctx->screen_height);

            SDL_BlendMode bm = SDL_ComposeCustomBlendMode(
                SDL_BLENDFACTOR_ONE,
                SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                SDL_BLENDOPERATION_ADD,
                SDL_BLENDFACTOR_ONE,
                SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                SDL_BLENDOPERATION_ADD
            );

            SDL_SetRenderDrawBlendMode(sdl_ctx->renderer, bm);
            SDL_SetTextureBlendMode(sdl_ctx->layer_data[layer], bm);
        }
        SDL_SetRenderTarget(renderer, sdl_ctx->layer_data[layer]);

        if (layer == 0 && ctx->background_enabled) {
            // Draw background
            if (!sdl_ctx->background) {
                sdl_ctx->background = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, ctx->screen_width, ctx->screen_height);
                SDL_Texture *old_target = SDL_GetRenderTarget(renderer);
                SDL_SetRenderTarget(renderer, sdl_ctx->background);
                vui_sdl_draw_background(ctx, renderer);
                SDL_SetRenderTarget(renderer, old_target);
            }
            SDL_RenderCopy(renderer, sdl_ctx->background, 0, 0);
        } else {
            vui_sdl_set_render_color(renderer, ctx->layer_color[layer]);
            SDL_RenderClear(renderer);
        }
    }

    // Draw rects
    for (int i = 0; i < ctx->rect_count; i++) {
        vui_rect_priv_t *rect = &ctx->rects[i];

        SDL_SetRenderTarget(renderer, sdl_ctx->layer_data[rect->layer]);

        SDL_Rect sr;
        sr.x = rect->x;
        sr.y = rect->y;
        sr.w = rect->w;
        sr.h = rect->h;
        vui_sdl_set_render_color(renderer, rect->color);

        if (rect->border_radius == 0) {
            SDL_RenderFillRect(renderer, &sr);
        } else {
            for (int y = 0; y < sr.h; y++) {
                float x_inset = calculate_x_inset(y, sr.h, rect->border_radius);
                float draw_y = sr.y + y;
                SDL_RenderDrawLineF(renderer, sr.x + x_inset, draw_y, sr.x + sr.w - x_inset, draw_y);
            }
        }
    }

    // Draw labels
    for (int i = 0; i < ctx->label_count; i++) {
        vui_label_t *lbl = &ctx->labels[i];
        if (lbl->text[0] && lbl->visible) {
            SDL_SetRenderTarget(renderer, sdl_ctx->layer_data[lbl->layer]);

            SDL_Color c;
            c.r = lbl->color.r * 0xFF;
            c.g = lbl->color.g * 0xFF;
            c.b = lbl->color.b * 0xFF;
            c.a = lbl->color.a * 0xFF;

            SDL_Surface *surface = TTF_RenderUTF8_Blended_Wrapped(get_font(sdl_ctx, lbl->size), lbl->text, c, lbl->w);

            SDL_Rect sr;
            sr.x = 0;
            sr.y = 0;
            sr.w = intmin(lbl->w, surface->w);
            sr.h = intmin(lbl->h, surface->h);

            SDL_Rect dr;
            dr.x = lbl->x;
            dr.y = lbl->y;
            dr.w = sr.w;
            dr.h = sr.h;

            SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_RenderCopy(renderer, texture, &sr, &dr);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
    }

    // Draw textedits
    for (int i = 0; i < ctx->textedit_count; i++) {
        vui_textedit_t *edit = &ctx->textedits[i];
        if (!edit->visible) {
            continue;
        }

        SDL_SetRenderTarget(renderer, sdl_ctx->layer_data[edit->layer]);

        SDL_Rect rect;
        rect.x = edit->x;
        rect.y = edit->y;
        rect.w = edit->w;
        rect.h = edit->h;

        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x80);

        const int bgrect_pad = 8;
        SDL_Rect bgrect = rect;
        bgrect.x -= bgrect_pad;
        bgrect.y -= bgrect_pad;
        bgrect.w += bgrect_pad + bgrect_pad;
        bgrect.h += bgrect_pad + bgrect_pad;

        for (int y = 0; y < bgrect.h; y++) {
            int x_inset = calculate_x_inset(y, bgrect.h, bgrect_pad);
            int ry = bgrect.y+y;
            SDL_RenderDrawLine(renderer, bgrect.x + x_inset, ry, bgrect.x + bgrect.w - x_inset, ry);
        }

        SDL_Color c;
        c.r = c.g = c.b = c.a = 0xFF;

        TTF_Font *font = get_font(sdl_ctx, edit->size);

        if (edit->text[0]) {

			if (edit->password) {
				if (!sdl_ctx->pw_tex) {
					sdl_ctx->pw_tex = vui_sdl_load_texture(sdl_ctx->renderer, "circle_big.svg");
				}

				uint8_t f = (edit->enabled ? 1 : 0.5f) * 0xFF;
				SDL_SetTextureAlphaMod(sdl_ctx->pw_tex, f);
				SDL_SetTextureColorMod(sdl_ctx->pw_tex, f, f, f);

				const char *cc = edit->text;
				SDL_Rect pwr;
				pwr.x = rect.x;
				pwr.y = rect.y + rect.h/2 - PW_CHAR_SIZE/2;
				pwr.w = PW_CHAR_SIZE;
				pwr.h = PW_CHAR_SIZE;
				while (*cc != 0) {
					SDL_RenderCopy(renderer, sdl_ctx->pw_tex, 0, &pwr);
					cc++;
					pwr.x += PW_CHAR_SIZE + PW_CHAR_PAD;
				}
			} else {
				SDL_Surface *surface = TTF_RenderUTF8_Blended(font, edit->text, c);
				SDL_Texture *texture = SDL_CreateTextureFromSurface(sdl_ctx->renderer, surface);
				SDL_Rect src;
				src.x = src.y = 0;

				src.w = surface->w;
				src.h = surface->h;

				if (src.w > rect.w) {
					src.w = rect.w;
				} else {
					rect.w = src.w;
				}

				uint8_t f = (edit->enabled ? 1 : 0.5f) * 0xFF;
				SDL_SetTextureAlphaMod(texture, f);
				SDL_SetTextureColorMod(texture, f, f, f);

				SDL_RenderCopy(renderer, texture, &src, &rect);

				SDL_FreeSurface(surface);
				SDL_DestroyTexture(texture);
			}
        }

        if (ctx->active_textedit == i) {
            struct timeval now;
            gettimeofday(&now, 0);
            time_t diff = (now.tv_sec - ctx->active_textedit_time.tv_sec) * 1000000 + (now.tv_usec - ctx->active_textedit_time.tv_usec);
            if ((diff % 1000000) < 500000) {
                char *copy_end = edit->text;
                for (int i = 0; i < edit->cursor; i++) {
                    copy_end = vui_utf8_advance(copy_end);
                }

                size_t len = copy_end - edit->text;

				int cursor_x;
				if (edit->password) {
					cursor_x = rect.x + (PW_CHAR_SIZE + PW_CHAR_PAD) * len - PW_CHAR_PAD/2;
				} else {
                	char tmp_ate[MAX_BUTTON_TEXT];
					strncpy(tmp_ate, edit->text, len);
					tmp_ate[len] = 0;

					int tw;
					TTF_SizeUTF8(font, tmp_ate, &tw, 0);
					cursor_x = rect.x + tw;
				}
				SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
				SDL_RenderDrawLine(renderer, cursor_x, rect.y, cursor_x, rect.y + rect.h);
            }
        }
    }

    // Draw buttons
    for (int i = 0; i < ctx->button_count; i++) {
        vui_button_t *btn = &ctx->buttons[i];

        if (!btn->visible) {
            continue;
        }

        SDL_SetRenderTarget(renderer, sdl_ctx->layer_data[btn->layer]);

        // Check if button needs to be redrawn
        vui_sdl_cached_texture_t *btn_tex = &sdl_ctx->button_cache[i];
        if (!btn_tex->texture || btn_tex->w != btn->w || btn_tex->h != btn->h || strcmp(btn_tex->text, btn->text) || strcmp(btn_tex->icon, btn->icon) || btn_tex->checked != btn->checked || btn_tex->icon_mod != btn->icon_mod) {
            // Must re-draw texture
            if (btn_tex->texture && (btn_tex->w != btn->w || btn_tex->h != btn->h)) {
                SDL_DestroyTexture(btn_tex->texture);
                btn_tex->texture = 0;
            }

            const int MSAA = 2;

            if (!btn_tex->texture) {
                btn_tex->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, btn->w * MSAA, btn->h * MSAA);
            }

            SDL_Texture *old_target = SDL_GetRenderTarget(renderer);
            SDL_SetRenderTarget(renderer, btn_tex->texture);
            vui_sdl_draw_button(ctx, sdl_ctx, btn, MSAA);
            SDL_SetRenderTarget(renderer, old_target);

            btn_tex->w = btn->w;
            btn_tex->h = btn->h;
            btn_tex->icon_mod = btn->icon_mod;
            strcpy(btn_tex->text, btn->text);
            strcpy(btn_tex->icon, btn->icon);

            btn_tex->checked = btn->checked;
        }

        // Draw cached texture onto screen
        SDL_Rect rect;
        rect.x = btn->sx;
        rect.y = btn->sy;
        rect.w = btn->sw;
        rect.h = btn->sh;

        uint8_t f = (btn->enabled ? 1 : 0.5f) * 0xFF;
        SDL_SetTextureAlphaMod(btn_tex->texture, f);
        SDL_SetTextureColorMod(btn_tex->texture, f, f, f);

        SDL_RenderCopy(renderer, btn_tex->texture, 0, &rect);

        if (i == ctx->selected_button) {
            const int btn_select_short_sz = 4;
            const int btn_select_long_sz = 24;

            const int tlx = btn->sx;
            const int tly = btn->sy;
            const int trx = btn->sx + btn->sw;
            const int try = tly;
            const int blx = tlx;
            const int bly = btn->sy + btn->sh;
            const int brx = trx;
            const int bry = bly;

            SDL_SetRenderDrawColor(renderer, 0x17, 0xCB, 0xD1, 0xFF);

            SDL_Rect r;

            r.x = tlx;
            r.y = tly;
            r.w = btn_select_long_sz;
            r.h = btn_select_short_sz;

            SDL_RenderFillRect(renderer, &r);

            r.x = trx - btn_select_long_sz;
            SDL_RenderFillRect(renderer, &r);

            r.y = bly - btn_select_short_sz;
            SDL_RenderFillRect(renderer, &r);

            r.x = tlx;
            SDL_RenderFillRect(renderer, &r);

            r.w = btn_select_short_sz;
            r.h = btn_select_long_sz;

            r.y = tly;
            SDL_RenderFillRect(renderer, &r);

            r.x = trx - btn_select_short_sz;
            SDL_RenderFillRect(renderer, &r);

            r.y = bry - btn_select_long_sz;
            SDL_RenderFillRect(renderer, &r);

            r.x = blx;
            SDL_RenderFillRect(renderer, &r);
        }
    }

    // Draw images
    for (int i = 0; i < MAX_BUTTON_COUNT; i++) {
        vui_image_t *img = &ctx->images[i];
        if (!img->valid || img->image[0] == 0) {
            continue;
        }

        SDL_SetRenderTarget(renderer, sdl_ctx->layer_data[img->layer]);

        // Find image in cache
        vui_sdl_cached_texture_t *img_tex = &sdl_ctx->image_cache[i];
        if (!img_tex->texture || strcmp(img_tex->icon, img->image)) {
            if (img_tex->texture) {
                SDL_DestroyTexture(img_tex->texture);
            }

            img_tex->texture = vui_sdl_load_texture(renderer, img->image);

            strcpy(img_tex->icon, img->image);
        }

        SDL_Rect dst;
        dst.x = img->x;
        dst.y = img->y;
        dst.w = img->w;
        dst.h = img->h;

        SDL_RenderCopy(renderer, img_tex->texture, 0, &dst);
    }
}

int get_texture_from_cpu_frame(vui_sdl_context_t *sdl_ctx, AVFrame *f)
{
	if (!sdl_ctx->game_tex) {
		sdl_ctx->game_tex = SDL_CreateTexture(
			sdl_ctx->renderer,
			SDL_PIXELFORMAT_IYUV,
			SDL_TEXTUREACCESS_STREAMING,
			f->width,
			f->height
		);
		if (!sdl_ctx->game_tex) {
			vpilog("Failed to create texture for CPU frame\n");
		}
	}
	// SDL_SetRenderTarget(sdl_ctx->renderer, sdl_ctx->game_tex);
	// SDL_SetRenderDrawColor(sdl_ctx->renderer, 0, 0, 0, 0);
	// SDL_RenderClear(sdl_ctx->renderer);
	int ret = SDL_UpdateYUVTexture(sdl_ctx->game_tex, NULL,
						f->data[0], f->linesize[0],
						f->data[1], f->linesize[1],
						f->data[2], f->linesize[2]);
	if (ret != 0) {
		vpilog("Failed to update YUV texture for CPU frame: %s\n", SDL_GetError());
		return 0;
	}

	return 1;
}

#ifdef VANILLA_HAS_EGL
int check_has_EGL_EXT_image_dma_buf_import()
{
	// Determine if we have this EGL extension or not
	const char *egl_extensions = eglQueryString(eglGetCurrentDisplay(), EGL_EXTENSIONS);
	if (!egl_extensions) {
		return 0;
	}

	char *extensions = SDL_strdup(egl_extensions);
	if (!extensions) {
		return 0;
	}

	char *saveptr, *token;
	token = SDL_strtokr(extensions, " ", &saveptr);
	if (!token) {
		SDL_free(extensions);
		return 0;
	}

	int ret = 0;
	do {
		if (SDL_strcmp(token, "EGL_EXT_image_dma_buf_import") == 0
			|| SDL_strcmp(token, "EGL_EXT_image_dma_buf_import_modifiers") == 0) {
			ret++;
		}
	} while ((token = SDL_strtokr(NULL, " ", &saveptr)) != NULL);

	SDL_free(extensions);

	return ret == 2;

	// if (SDL_GL_ExtensionSupported("GL_OES_EGL_image")) {
	// 	glEGLImageTargetTexture2DOESFunc = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
	// }

	// glActiveTextureARBFunc = (PFNGLACTIVETEXTUREARBPROC)SDL_GL_GetProcAddress("glActiveTextureARB");

	// if (has_EGL_EXT_image_dma_buf_import &&
	// 	glEGLImageTargetTexture2DOESFunc &&
	// 	glActiveTextureARBFunc) {
	// 	has_eglCreateImage = true;
	// }
}
#endif // VANILLA_HAS_EGL

int get_texture_from_drm_prime_frame(vui_sdl_context_t *sdl_ctx, AVFrame *f)
{
#ifdef VANILLA_HAS_EGL
	const AVDRMFrameDescriptor *desc = (const AVDRMFrameDescriptor *)f->data[0];

	static PFNGLACTIVETEXTUREARBPROC glActiveTextureARB = NULL;
	if (!glActiveTextureARB) {
		glActiveTextureARB = SDL_GL_GetProcAddress("glActiveTextureARB");
	}

	static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = NULL;
	if (!glEGLImageTargetTexture2DOES) {
		glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
	}

	int image_index = 0;
    EGLDisplay display = eglGetCurrentDisplay();

	for (int i = 0; i < desc->nb_layers; i++) {
		const AVDRMLayerDescriptor *layer = &desc->layers[i];

		for (int j = 0; j < layer->nb_planes; j++) {
			const AVDRMPlaneDescriptor *plane = &layer->planes[j];
			const AVDRMObjectDescriptor *object = &desc->objects[plane->object_index];

            static int has_EGL_EXT_image_dma_buf_import = -1;
            if (has_EGL_EXT_image_dma_buf_import == -1) {
                has_EGL_EXT_image_dma_buf_import = check_has_EGL_EXT_image_dma_buf_import();
            }

            const EGLAttrib EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT_OR_NONE =
                has_EGL_EXT_image_dma_buf_import && object->format_modifier != DRM_FORMAT_MOD_INVALID ?
                    EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT : EGL_NONE;

            if (!sdl_ctx->game_tex) {
                sdl_ctx->game_tex = SDL_CreateTexture(
                    sdl_ctx->renderer,
                    layer->format == DRM_FORMAT_YUV420 ? SDL_PIXELFORMAT_IYUV : SDL_PIXELFORMAT_NV12,
                    SDL_TEXTUREACCESS_STATIC,
                    f->width,
                    f->height
                );

                if (!sdl_ctx->game_tex) {
                    vpilog("Failed to create texture for DRM PRIME frame\n");
                    return 0;
                }
            }

            EGLAttrib use_fmt = layer->format == DRM_FORMAT_YUV420 ? DRM_FORMAT_R8 : layer->format;

            int w = f->width;
            int h = f->height;
            if (image_index > 0) {
                w /= 2;
                h /= 2;
            }

            EGLAttrib attr[] = {
                EGL_LINUX_DRM_FOURCC_EXT, 					use_fmt,
                EGL_WIDTH,									w,
                EGL_HEIGHT,									h,
                EGL_DMA_BUF_PLANE0_FD_EXT,					object->fd,
                EGL_DMA_BUF_PLANE0_OFFSET_EXT,				plane->offset,
                EGL_DMA_BUF_PLANE0_PITCH_EXT,				plane->pitch,
                EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT_OR_NONE,	(object->format_modifier >> 0) & 0xFFFFFFFF,
                EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT,			(object->format_modifier >> 32) & 0xFFFFFFFF,
                EGL_NONE
            };

            EGLImage image = eglCreateImage(display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, 0, attr);
            if (image == EGL_NO_IMAGE) {
                vpilog("Failed to create EGLImage: 0x%x (display: %p, layer %i, plane %i)\n", eglGetError(), display, i, j);
                return 0;
            }

			SDL_GL_BindTexture(sdl_ctx->game_tex, 0, 0);
			glActiveTextureARB(GL_TEXTURE0_ARB + image_index);
			glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
			image_index++;
			SDL_GL_UnbindTexture(sdl_ctx->game_tex);

            eglDestroyImage(display, image);
		}
	}

	return 1;
#else
	vpilog("No EGL support to display VAAPI texture\n");
	return 0;
#endif
}

// Rendering/main thread
int vui_update_sdl(vui_context_t *vui)
{
    static Uint32 last_update_time = 0;

    vui_sdl_event_thread(vui);

    vui_sdl_context_t *sdl_ctx = (vui_sdl_context_t *) vui->platform_data;

    SDL_Rect *dst_rect = &sdl_ctx->dst_rect;

    SDL_Renderer *renderer = sdl_ctx->renderer;

    vui_update(vui);

    SDL_Texture *main_tex;

#ifdef VANILLA_DRM_AVAILABLE
    static vanilla_drm_ctx_t *drm_ctx = NULL;
#endif // VANILLA_DRM_AVAILABLE

    int handle_final_blit = 1;
    if (!vui->game_mode) {

#ifdef VANILLA_DRM_AVAILABLE
        if (drm_ctx) {
            vui_sdl_drm_free(&drm_ctx); // will set to null
        }
#endif // VANILLA_DRM_AVAILABLE

        // Draw vui to a custom texture
        vui_draw_sdl(vui, renderer);

        // Flatten layers
		int el[MAX_BUTTON_COUNT];
		int el_count = 0;
		for (int i = 0; i < vui->layers; i++) {
			if (vui->layer_enabled[i]) {
				el[el_count] = i;
				el_count++;
			}
		}

        for (int i = el_count - 1; i > 0; i--) {
            SDL_Texture *bg = sdl_ctx->layer_data[el[i-1]];
            SDL_Texture *fg = sdl_ctx->layer_data[el[i]];

            SDL_SetRenderTarget(renderer, bg);
            SDL_SetTextureColorMod(fg, vui->layer_opacity[i] * 0xFF, vui->layer_opacity[i] * 0xFF, vui->layer_opacity[i] * 0xFF);
            SDL_SetTextureAlphaMod(fg, vui->layer_opacity[i] * 0xFF);
            SDL_RenderCopy(renderer, fg, NULL, NULL);
        }

        main_tex = sdl_ctx->layer_data[0];
    } else {
        pthread_mutex_lock(&vpi_present_frame_mutex);
		if (vpi_present_frame && vpi_present_frame->format != -1) {
			av_frame_move_ref(sdl_ctx->frame, vpi_present_frame);
            sdl_ctx->frame->pts = vpi_present_frame->pts;
		}
        pthread_mutex_unlock(&vpi_present_frame_mutex);

		if (sdl_ctx->frame->format != -1) {
            switch (sdl_ctx->frame->format) {
            case AV_PIX_FMT_DRM_PRIME:
            {
                // get_texture_from_drm_prime_frame(sdl_ctx, sdl_ctx->frame);

#ifdef VANILLA_DRM_AVAILABLE
                if (!drm_ctx) {
                    vui_sdl_drm_initialize(&drm_ctx, sdl_ctx->window);
                }
                vui_sdl_drm_present(drm_ctx, sdl_ctx->frame);
                handle_final_blit = 0;
#endif // VANILLA_DRM_AVAILABLE
                break;
            }
            case AV_PIX_FMT_VAAPI:
			{
				AVFrame *drm = av_frame_alloc();
				if (drm) {
					drm->format = AV_PIX_FMT_DRM_PRIME;
					if (av_hwframe_map(drm, sdl_ctx->frame, 0) >= 0) {
						get_texture_from_drm_prime_frame(sdl_ctx, drm);
					} else {
						vpilog("Failed to map DRM PRIME frame from VAAPI\n");
					}
					av_frame_free(&drm);
				} else {
					vpilog("Failed to allocate DRM PRIME frame from VAAPI\n");
				}
                break;
			}
            case AV_PIX_FMT_YUV420P:
				get_texture_from_cpu_frame(sdl_ctx, sdl_ctx->frame);
                break;
            }
			av_frame_unref(sdl_ctx->frame);

            if (sdl_ctx->game_tex) {
                if (handle_final_blit) {
                    main_tex = sdl_ctx->layer_data[0];
                    SDL_SetRenderTarget(renderer, main_tex);
                    SDL_RenderCopy(renderer, sdl_ctx->game_tex, 0, 0);
                } else {
                    main_tex = sdl_ctx->game_tex;
                }
            }
		} else {
            // Didn't get a frame, nothing to be done
            handle_final_blit = 0;
        }
    }

    if (handle_final_blit) {
        // Draw toast on screen if necessary
        const int TOAST_PADDING = 12;
        int cur_toast;
        vpi_get_toast(&cur_toast, 0, 0, 0);
        if (cur_toast != sdl_ctx->last_shown_toast) {
            // Get toast information
            char toast_str[VPI_TOAST_MAX_LEN];
            vpi_get_toast(&cur_toast, toast_str, sizeof(toast_str), &sdl_ctx->toast_expiry);

            sdl_ctx->last_shown_toast = cur_toast;

            const int toast_w = vui->screen_width/2;

            SDL_Color c;
            c.r = c.g = c.b = 0x40;
            c.a = 0xFF;

            SDL_Surface *surface = TTF_RenderUTF8_Blended_Wrapped(sdl_ctx->sysfont_small, toast_str, c, toast_w - TOAST_PADDING - TOAST_PADDING);
            SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

            const int toast_h = surface->h + TOAST_PADDING + TOAST_PADDING;
            sdl_ctx->toast_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, toast_w, toast_h);
            SDL_SetTextureBlendMode(sdl_ctx->toast_tex, SDL_BLENDMODE_BLEND);

            SDL_SetRenderTarget(renderer, sdl_ctx->toast_tex);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
            SDL_RenderClear(renderer);

            SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xF0);
            const int TOAST_BORDER_RADIUS = TOAST_PADDING;
            for (int y = 0; y < toast_h; y++) {
                const int x_inset = calculate_x_inset(y, toast_h, TOAST_BORDER_RADIUS);
                SDL_RenderDrawLine(renderer, x_inset, y, toast_w - x_inset, y);
            }

            SDL_Rect dst_rect;
            dst_rect.x = dst_rect.y = TOAST_PADDING;
            dst_rect.w = surface->w;
            dst_rect.h = surface->h;
            SDL_RenderCopy(renderer, texture, 0, &dst_rect);

            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
        if (sdl_ctx->toast_tex) {
            // Draw toast to screen
            SDL_SetRenderTarget(renderer, main_tex);

            // Get texture size
            int toast_w, toast_h;
            SDL_QueryTexture(sdl_ctx->toast_tex, 0, 0, &toast_w, &toast_h);

            // Calculate dst rect
            SDL_Rect dst_rect;
            dst_rect.w = toast_w;
            dst_rect.h = toast_h;
            dst_rect.y = vui->screen_height - dst_rect.h - TOAST_PADDING - TOAST_PADDING;
            dst_rect.x = vui->screen_width/2 - toast_w/2;

            SDL_RenderCopy(renderer, sdl_ctx->toast_tex, 0, &dst_rect);

            // Handle expiry
            struct timeval now;
            gettimeofday(&now, 0);
            if (now.tv_sec*1000000+now.tv_usec >= sdl_ctx->toast_expiry.tv_sec*1000000+sdl_ctx->toast_expiry.tv_usec) {
                SDL_DestroyTexture(sdl_ctx->toast_tex);
                sdl_ctx->toast_tex = 0;
            }
        }

        // Calculate our destination rectangle
        int win_w, win_h;
        SDL_GetWindowSize(sdl_ctx->window, &win_w, &win_h);
        if (win_w == vui->screen_width && win_h == vui->screen_height) {
            dst_rect->x = 0;
            dst_rect->y = 0;
            dst_rect->w = vui->screen_width;
            dst_rect->h = vui->screen_height;
        } else if (win_w * 100 / win_h > vui->screen_width * 100 / vui->screen_height) {
            // Window is wider than texture, scale by height
            dst_rect->h = win_h;
            dst_rect->y = 0;
            dst_rect->w = win_h * vui->screen_width / vui->screen_height;
            dst_rect->x = win_w / 2 - dst_rect->w / 2;
        } else {
            // Window is taller than texture, scale by width
            dst_rect->w = win_w;
            dst_rect->x = 0;
            dst_rect->h = win_w * vui->screen_height / vui->screen_width;
            dst_rect->y = win_h / 2 - dst_rect->h / 2;
        }

        // Copy texture to window
        SDL_SetRenderTarget(renderer, NULL);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, main_tex, NULL, dst_rect);

        // Flip surfaces
        SDL_RenderPresent(renderer);
    }

    // Frame limiter to save CPU cycles
    const Uint32 target = 5; // No need to update faster than 200Hz (gamepad polls at 180Hz, but this is easier to calculate)
    Uint32 frame_delta = SDL_GetTicks() - last_update_time;
    if (frame_delta < target) {
        SDL_Delay(target - frame_delta);
    }
    last_update_time = SDL_GetTicks();

    return !vui->quit;
}

