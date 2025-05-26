#include "ui_sdl.h"

#include <math.h>
#include <SDL2/SDL.h>
#include <SDL_image.h>
#include <SDL_power.h>
#include <SDL_ttf.h>
#include <pthread.h>
#include <vanilla.h>

#include "game/game_decode.h"
#include "game/game_main.h"
#include "menu/menu.h"
#include "platform.h"
#include "ui_priv.h"
#include "ui_util.h"

#define MIN(a,b) (((a)<(b))?(a):(b))

typedef struct {
    SDL_Texture *texture;
    int w;
    int h;
    char text[MAX_BUTTON_TEXT];
    char icon[MAX_BUTTON_TEXT];
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
    SDL_Texture *game_tex;
    int last_shown_toast;
    SDL_Texture *toast_tex;
    struct timeval toast_expiry;
    AVFrame *frame;

	uint8_t *audio_buffer;
	size_t audio_buffer_size;
	size_t audio_buffer_start;
	size_t audio_buffer_end;
	pthread_mutex_t audio_buffer_mutex;
} vui_sdl_context_t;

static int button_map[SDL_CONTROLLER_BUTTON_MAX];
static int axis_map[SDL_CONTROLLER_AXIS_MAX];
static int key_map[SDL_NUM_SCANCODES];
static int vibrate = 0;

void init_gamepad()
{
	vibrate = 0;

    // Initialize arrays to -1
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++) button_map[i] = -1;
    for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++) axis_map[i] = -1;
    for (int i = 0; i < SDL_NUM_SCANCODES; i++) key_map[i] = -1;

    button_map[SDL_CONTROLLER_BUTTON_A] = VANILLA_BTN_A;
    button_map[SDL_CONTROLLER_BUTTON_B] = VANILLA_BTN_B;
    button_map[SDL_CONTROLLER_BUTTON_X] = VANILLA_BTN_X;
    button_map[SDL_CONTROLLER_BUTTON_Y] = VANILLA_BTN_Y;
    button_map[SDL_CONTROLLER_BUTTON_BACK] = VANILLA_BTN_MINUS;
    button_map[SDL_CONTROLLER_BUTTON_GUIDE] = VANILLA_BTN_HOME;
    button_map[SDL_CONTROLLER_BUTTON_MISC1] = VANILLA_BTN_TV;
    button_map[SDL_CONTROLLER_BUTTON_START] = VANILLA_BTN_PLUS;
    button_map[SDL_CONTROLLER_BUTTON_LEFTSTICK] = VANILLA_BTN_L3;
    button_map[SDL_CONTROLLER_BUTTON_RIGHTSTICK] = VANILLA_BTN_R3;
    button_map[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] = VANILLA_BTN_L;
    button_map[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = VANILLA_BTN_R;
    button_map[SDL_CONTROLLER_BUTTON_DPAD_UP] = VANILLA_BTN_UP;
    button_map[SDL_CONTROLLER_BUTTON_DPAD_DOWN] = VANILLA_BTN_DOWN;
    button_map[SDL_CONTROLLER_BUTTON_DPAD_LEFT] = VANILLA_BTN_LEFT;
    button_map[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] = VANILLA_BTN_RIGHT;
    axis_map[SDL_CONTROLLER_AXIS_LEFTX] = VANILLA_AXIS_L_X;
    axis_map[SDL_CONTROLLER_AXIS_LEFTY] = VANILLA_AXIS_L_Y;
    axis_map[SDL_CONTROLLER_AXIS_RIGHTX] = VANILLA_AXIS_R_X;
    axis_map[SDL_CONTROLLER_AXIS_RIGHTY] = VANILLA_AXIS_R_Y;
    axis_map[SDL_CONTROLLER_AXIS_TRIGGERLEFT] = VANILLA_BTN_ZL;
    axis_map[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] = VANILLA_BTN_ZR;

    key_map[SDL_SCANCODE_UP] = VANILLA_BTN_UP;
    key_map[SDL_SCANCODE_DOWN] = VANILLA_BTN_DOWN;
    key_map[SDL_SCANCODE_LEFT] = VANILLA_BTN_LEFT;
    key_map[SDL_SCANCODE_RIGHT] = VANILLA_BTN_RIGHT;
    key_map[SDL_SCANCODE_Z] = VANILLA_BTN_A;
    key_map[SDL_SCANCODE_X] = VANILLA_BTN_B;
    key_map[SDL_SCANCODE_C] = VANILLA_BTN_X;
    key_map[SDL_SCANCODE_V] = VANILLA_BTN_Y;
    key_map[SDL_SCANCODE_RETURN] = VANILLA_BTN_PLUS;
    key_map[SDL_SCANCODE_LCTRL] = VANILLA_BTN_MINUS;
    key_map[SDL_SCANCODE_H] = VANILLA_BTN_HOME;
    key_map[SDL_SCANCODE_Y] = VANILLA_BTN_TV;
    key_map[SDL_SCANCODE_W] = VANILLA_AXIS_L_UP;
    key_map[SDL_SCANCODE_A] = VANILLA_AXIS_L_LEFT;
    key_map[SDL_SCANCODE_S] = VANILLA_AXIS_L_DOWN;
    key_map[SDL_SCANCODE_D] = VANILLA_AXIS_L_RIGHT;
    key_map[SDL_SCANCODE_E] = VANILLA_BTN_L3;
    key_map[SDL_SCANCODE_KP_8] = VANILLA_AXIS_R_UP;
    key_map[SDL_SCANCODE_KP_4] = VANILLA_AXIS_R_LEFT;
    key_map[SDL_SCANCODE_KP_2] = VANILLA_AXIS_R_DOWN;
    key_map[SDL_SCANCODE_KP_6] = VANILLA_AXIS_R_RIGHT;
    key_map[SDL_SCANCODE_KP_5] = VANILLA_BTN_R3;
    key_map[SDL_SCANCODE_T] = VANILLA_BTN_L;
    key_map[SDL_SCANCODE_G] = VANILLA_BTN_ZL;
    key_map[SDL_SCANCODE_U] = VANILLA_BTN_R;
    key_map[SDL_SCANCODE_J] = VANILLA_BTN_ZR;

    key_map[SDL_SCANCODE_F5] = VPI_ACTION_TOGGLE_RECORDING;
    key_map[SDL_SCANCODE_F12] = VPI_ACTION_SCREENSHOT;
    key_map[SDL_SCANCODE_ESCAPE] = VPI_ACTION_DISCONNECT;
}

SDL_GameController *find_valid_controller()
{
	SDL_GameController *c = NULL;

	vpilog("Looking for game controllers...\n");
	for (int i = 0; i < SDL_NumJoysticks(); i++) {
		vpilog("  Found %i: %s\n", i, SDL_GameControllerNameForIndex(i));
		if (!c) {
			c = SDL_GameControllerOpen(i);
			if (c) {
				// Enable gyro/accelerometer
				SDL_GameControllerSetSensorEnabled(c, SDL_SENSOR_ACCEL, 1);
				SDL_GameControllerSetSensorEnabled(c, SDL_SENSOR_GYRO, 1);
			}
		}
	}

	if (c) {
		vpilog("Using game controller \"%s\"\n", SDL_GameControllerName(c));
	}

	return c;
}

void vui_sdl_audio_handler(const void *data, size_t size, void *userdata)
{
	vui_sdl_context_t *sdl_ctx = (vui_sdl_context_t *) userdata;

	if (!sdl_ctx->audio) {
		// Buffers will not have been initialized, so return before we try using them
		return;
	}

	pthread_mutex_lock(&sdl_ctx->audio_buffer_mutex);

	for (size_t i = 0; i < size; ) {
		size_t phys = sdl_ctx->audio_buffer_end % sdl_ctx->audio_buffer_size;
		size_t max_write = MIN(sdl_ctx->audio_buffer_size - phys, size - i);
		memcpy(sdl_ctx->audio_buffer + phys, ((const unsigned char *) data) + i, max_write);

		i += max_write;
		sdl_ctx->audio_buffer_end += max_write;

		if (sdl_ctx->audio_buffer_end > sdl_ctx->audio_buffer_start + sdl_ctx->audio_buffer_size) {
			vpilog("SKIPPED %zu AUDIO BYTES\n", (sdl_ctx->audio_buffer_end - sdl_ctx->audio_buffer_size) - sdl_ctx->audio_buffer_start);
			sdl_ctx->audio_buffer_start = sdl_ctx->audio_buffer_end - sdl_ctx->audio_buffer_size;
		}
	}

	pthread_mutex_unlock(&sdl_ctx->audio_buffer_mutex);
}

void vui_sdl_vibrate_handler(uint8_t vibrate, void *userdata)
{
    vui_sdl_context_t *sdl_ctx = (vui_sdl_context_t *) userdata;
    if (sdl_ctx->controller) {
        uint16_t amount = vibrate ? 0xFFFF : 0;
		SDL_GameControllerRumble(sdl_ctx->controller, amount, amount, 0);
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

vui_power_state_t vui_sdl_power_state_handler(int *percent)
{
    return (vui_power_state_t) SDL_GetPowerInfo(NULL, percent);
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

	pthread_mutex_lock(&sdl_ctx->audio_buffer_mutex);

	if ((sdl_ctx->audio_buffer_end - sdl_ctx->audio_buffer_start) >= len) {
		for (size_t i = 0; i < len; ) {
			size_t phys = sdl_ctx->audio_buffer_start % sdl_ctx->audio_buffer_size;
			size_t max_write = MIN(sdl_ctx->audio_buffer_size - phys, len - i);
			memcpy(stream + i, sdl_ctx->audio_buffer + phys, max_write);
			i += max_write;
			sdl_ctx->audio_buffer_start += max_write;
		}
	} else {
		// Return silence
		memset(stream, 0, len);
	}

	pthread_mutex_unlock(&sdl_ctx->audio_buffer_mutex);
}

void mic_callback(void *userdata, Uint8 *stream, int len)
{
	vui_context_t *ctx = (vui_context_t *) userdata;
	if (ctx->mic_callback)
    	ctx->mic_callback(ctx->mic_callback_data, stream, len);
}

int vui_init_sdl(vui_context_t *ctx, int fullscreen)
{
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        vpilog("Failed to initialize SDL: %s\n", SDL_GetError());
        return -1;
    }

    vui_sdl_context_t *sdl_ctx = malloc(sizeof(vui_sdl_context_t));
    ctx->platform_data = sdl_ctx;

    memset(&sdl_ctx->dst_rect, 0, sizeof(SDL_Rect));

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");

    SDL_WindowFlags window_flags;
    if (fullscreen) {
        window_flags = SDL_WINDOW_FULLSCREEN;
        SDL_ShowCursor(0);
    } else {
        window_flags = SDL_WINDOW_RESIZABLE;
    }

    sdl_ctx->window = SDL_CreateWindow("Vanilla", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ctx->screen_width, ctx->screen_height, window_flags);
    if (!sdl_ctx->window) {
        vpilog("Failed to CreateWindow\n");
        return -1;
    }

    sdl_ctx->renderer = SDL_CreateRenderer(sdl_ctx->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdl_ctx->renderer) {
        vpilog("Failed to CreateRenderer\n");
        return -1;
    }

    // Open audio output device
    SDL_AudioSpec desired = {0}, obtained;
    desired.freq = 48000;
    desired.format = AUDIO_S16LSB;
    desired.channels = 2;
	desired.callback = audio_callback;
	desired.userdata = sdl_ctx;

    sdl_ctx->audio = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (sdl_ctx->audio) {
		// Set up buffers
		size_t buffer_size = obtained.samples * 4; // * 2 for 16-bit, * 2 for stereo
		buffer_size += 2048; // Pad buffer for maximum size of Wii U packet
		sdl_ctx->audio_buffer_size = buffer_size;
		sdl_ctx->audio_buffer = malloc(buffer_size);
		sdl_ctx->audio_buffer_start = 0;
		sdl_ctx->audio_buffer_end = 0;
		pthread_mutex_init(&sdl_ctx->audio_buffer_mutex, 0);

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

    ctx->font_height_handler = vui_sdl_font_height_handler;
    ctx->font_height_handler_data = sdl_ctx;

    ctx->text_open_handler = vui_sdl_text_open_handler;

    ctx->power_state_handler = vui_sdl_power_state_handler;

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

    sdl_ctx->game_tex = SDL_CreateTexture(sdl_ctx->renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, ctx->screen_width, ctx->screen_height);
    SDL_SetRenderTarget(sdl_ctx->renderer, sdl_ctx->game_tex);
    SDL_SetRenderDrawColor(sdl_ctx->renderer, 0, 0, 0, 0);
    SDL_RenderClear(sdl_ctx->renderer);

    sdl_ctx->background = 0;
    sdl_ctx->last_shown_toast = 0;
    sdl_ctx->toast_tex = 0;
    memset(sdl_ctx->layer_data, 0, sizeof(sdl_ctx->layer_data));
    memset(sdl_ctx->label_data, 0, sizeof(sdl_ctx->label_data));
    memset(sdl_ctx->button_cache, 0, sizeof(sdl_ctx->button_cache));
    memset(sdl_ctx->image_cache, 0, sizeof(sdl_ctx->image_cache));
    memset(sdl_ctx->textedit_cache, 0, sizeof(sdl_ctx->textedit_cache));
    sdl_ctx->controller = find_valid_controller();

    sdl_ctx->frame = av_frame_alloc();

	// Initialize gamepad lookup tables
	init_gamepad();

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

		free(sdl_ctx->audio_buffer);
		pthread_mutex_destroy(&sdl_ctx->audio_buffer_mutex);
	}

	if (sdl_ctx->mic) {
		SDL_CloseAudioDevice(sdl_ctx->mic);
	}

    SDL_DestroyTexture(sdl_ctx->toast_tex);
    SDL_DestroyTexture(sdl_ctx->game_tex);
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

void vui_sdl_draw_button(vui_context_t *vui, vui_sdl_context_t *sdl_ctx, vui_button_t *btn)
{
    SDL_SetRenderDrawColor(sdl_ctx->renderer, 0, 0, 0, 0);
    SDL_RenderClear(sdl_ctx->renderer);

    SDL_Color text_color;

    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = btn->w;
    rect.h = btn->h;

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
    int icon_size = btn->style == VUI_BUTTON_STYLE_CORNER ? rect.h * 1 / 3 : rect.h * 2 / 4;
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
        text_font = sdl_ctx->sysfont;
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

        text_rect.w = surface->w;
        text_rect.h = surface->h;

        const int btn_padding = 8;
        text_rect.x = rect.x + rect.w / 2 - text_rect.w / 2;
        text_rect.y = rect.y + rect.h / 2 - text_rect.h / 2;

        // Adjust text rect if icon exists
        if (icon) {
            if (btn->style == VUI_BUTTON_STYLE_CORNER) {
                text_rect.y = rect.y + rect.h/2 - (text_rect.h + icon_size + btn_padding)/2;
                icon_x = rect.x + rect.w/2 - icon_size/2;
                icon_y = text_rect.y + text_rect.h + btn_padding;
            } else {
                icon_x = rect.x + rect.w/2 - (text_rect.w + icon_size)/2;
                icon_y = rect.y + rect.h/2 - icon_size/2;
                text_rect.x = icon_x + icon_size + btn_padding;
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

            SDL_RenderCopy(renderer, texture, &src, &rect);

            SDL_FreeSurface(surface);
            SDL_DestroyTexture(texture);
        }

        if (ctx->active_textedit == i) {
            struct timeval now;
            gettimeofday(&now, 0);
            time_t diff = (now.tv_sec - ctx->active_textedit_time.tv_sec) * 1000000 + (now.tv_usec - ctx->active_textedit_time.tv_usec);
            if ((diff % 1000000) < 500000) {
                char tmp_ate[MAX_BUTTON_TEXT];

                char *copy_end = edit->text;
                for (int i = 0; i < edit->cursor; i++) {
                    copy_end = vui_utf8_advance(copy_end);
                }

                size_t len = copy_end - edit->text;
                strncpy(tmp_ate, edit->text, len);
                tmp_ate[len] = 0;

                int tw;
                TTF_SizeUTF8(font, tmp_ate, &tw, 0);
                int cursor_x = rect.x + tw;

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
        if (!btn_tex->texture || btn_tex->w != btn->w || btn_tex->h != btn->h || strcmp(btn_tex->text, btn->text) || strcmp(btn_tex->icon, btn->icon) || btn_tex->checked != btn->checked) {
            // Must re-draw texture
            if (btn_tex->texture && (btn_tex->w != btn->w || btn_tex->h != btn->h)) {
                SDL_DestroyTexture(btn_tex->texture);
                btn_tex->texture = 0;
            }

            if (!btn_tex->texture) {
                btn_tex->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, btn->w, btn->h);
            }

            SDL_Texture *old_target = SDL_GetRenderTarget(renderer);
            SDL_SetRenderTarget(renderer, btn_tex->texture);
            vui_sdl_draw_button(ctx, sdl_ctx, btn);
            SDL_SetRenderTarget(renderer, old_target);

            btn_tex->w = btn->w;
            btn_tex->h = btn->h;
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
        if (!img->valid) {
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

int32_t pack_float(float f)
{
    int32_t x;
    memcpy(&x, &f, sizeof(int32_t));
    return x;
}

int vui_update_sdl(vui_context_t *vui)
{
    vui_sdl_context_t *sdl_ctx = (vui_sdl_context_t *) vui->platform_data;

    SDL_Rect *dst_rect = &sdl_ctx->dst_rect;

    SDL_Renderer *renderer = sdl_ctx->renderer;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            vanilla_stop();
            return 0;
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            // Ensure dst_rect is initialized
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
        case SDL_CONTROLLERDEVICEADDED:
            // Attempt to find controller if one doesn't exist already
            if (!sdl_ctx->controller) {
                sdl_ctx->controller = find_valid_controller();
            }
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            // Handle current controller being removed
            if (sdl_ctx->controller && ev.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(sdl_ctx->controller))) {
                SDL_GameControllerClose(sdl_ctx->controller);
                sdl_ctx->controller = find_valid_controller();
            }
            break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            if (sdl_ctx->controller && ev.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(sdl_ctx->controller))) {
                int vanilla_btn = button_map[ev.cbutton.button];
                if (vanilla_btn > VPI_ACTION_START_INDEX) {
                    if (ev.type == SDL_CONTROLLERBUTTONDOWN)
                        vpi_menu_action(vui, (vpi_extra_action_t) vanilla_btn);
                } else if (vanilla_btn != -1) {
                    if (vui->game_mode) {
                        vanilla_set_button(vanilla_btn, ev.type == SDL_CONTROLLERBUTTONDOWN ? INT16_MAX : 0);
                    } else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                        vui_process_keydown(vui, vanilla_btn);
                    } else {
                        vui_process_keyup(vui, vanilla_btn);
                    }
                }
            }
            break;
        case SDL_CONTROLLERAXISMOTION:
            if (sdl_ctx->controller && ev.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(sdl_ctx->controller))) {
                int vanilla_axis = axis_map[ev.caxis.axis];
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
            if (vui->active_textedit == -1) {
                int vanilla_btn = key_map[ev.key.keysym.scancode];
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

    vui_update(vui);

    SDL_Texture *main_tex;

    if (!vui->game_mode) {
        // Draw vui to a custom texture
        vui_draw_sdl(vui, renderer);

        // Flatten layers
        for (int i = vui->layers - 1; i > 0; i--) {
            SDL_Texture *bg = sdl_ctx->layer_data[i-1];
            SDL_Texture *fg = sdl_ctx->layer_data[i];

            SDL_SetRenderTarget(renderer, bg);
            SDL_SetTextureColorMod(fg, vui->layer_opacity[i] * 0xFF, vui->layer_opacity[i] * 0xFF, vui->layer_opacity[i] * 0xFF);
            SDL_SetTextureAlphaMod(fg, vui->layer_opacity[i] * 0xFF);
            SDL_RenderCopy(renderer, fg, NULL, NULL);
        }

        main_tex = sdl_ctx->layer_data[0];
    } else {
        pthread_mutex_lock(&vpi_decoding_mutex);
		if (vpi_present_frame && vpi_present_frame->data[0]) {
			av_frame_move_ref(sdl_ctx->frame, vpi_present_frame);
		}
		pthread_mutex_unlock(&vpi_decoding_mutex);

		if (sdl_ctx->frame->data[0]) {
			SDL_UpdateYUVTexture(sdl_ctx->game_tex, NULL,
								 sdl_ctx->frame->data[0], sdl_ctx->frame->linesize[0],
								 sdl_ctx->frame->data[1], sdl_ctx->frame->linesize[1],
								 sdl_ctx->frame->data[2], sdl_ctx->frame->linesize[2]);
			av_frame_unref(sdl_ctx->frame);
		}
        main_tex = sdl_ctx->layer_data[0];

        SDL_SetRenderTarget(renderer, main_tex);
        SDL_RenderCopy(renderer, sdl_ctx->game_tex, 0, 0);
        // main_tex = sdl_ctx->game_tex;
    }

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

    return !vui->quit;
}
