#include "ui_sdl2.h"

#include <SDL2/SDL_ttf.h>
#include <math.h>

#include "ui_priv.h"

static TTF_Font *sysfont = 0;
static TTF_Font *sysfont_mini = 0;

typedef struct {
    SDL_Texture *texture;
    int w;
    int h;
    char text[MAX_BUTTON_TEXT];
} vui_sdl2_cached_texture_t;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Rect dst_rect;
    SDL_Texture *background;
    SDL_Texture *layer_data[MAX_BUTTON_COUNT];
    vui_sdl2_cached_texture_t button_cache[MAX_BUTTON_COUNT];
} vui_sdl2_context_t;

int intmin(int a, int b) { return a < b ? a : b; }
int intmax(int a, int b) { return a > b ? a : b; }

int vui_init_sdl2(vui_context_t *ctx)
{
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "Failed to initialize SDL\n");
        return -1;
    }

    vui_sdl2_context_t *sdl2_ctx = malloc(sizeof(vui_sdl2_context_t));
    ctx->platform_data = sdl2_ctx;

    memset(&sdl2_ctx->dst_rect, 0, sizeof(SDL_Rect));

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");

    sdl2_ctx->window = SDL_CreateWindow("Vanilla", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ctx->screen_width, ctx->screen_height, SDL_WINDOW_RESIZABLE);
    if (!sdl2_ctx->window) {
        return -1;
    }

    sdl2_ctx->renderer = SDL_CreateRenderer(sdl2_ctx->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdl2_ctx->renderer) {
        return -1;
    }

    // Enable blending
    SDL_SetRenderDrawBlendMode(sdl2_ctx->renderer, SDL_BLENDMODE_BLEND);

    if (TTF_Init()) {
        return -1;
    }

    const char *font_filename = "/home/matt/src/vanilla/rpi/font/system.ttf";

    sysfont = TTF_OpenFont(font_filename, 36);
    if (!sysfont) {
        return -1;
    }

    sysfont_mini = TTF_OpenFont(font_filename, 18);
    if (!sysfont_mini) {
        return -1;
    }

    sdl2_ctx->background = 0;
    memset(sdl2_ctx->layer_data, 0, sizeof(sdl2_ctx->layer_data));
    memset(sdl2_ctx->button_cache, 0, sizeof(sdl2_ctx->button_cache));

    return 0;
}

void vui_close_sdl2(vui_context_t *ctx)
{
    vui_sdl2_context_t *sdl2_ctx = (vui_sdl2_context_t *) ctx->platform_data;
    if (!sdl2_ctx) {
        return;
    }

    for (int i = 0; i < MAX_BUTTON_COUNT; i++) {
        if (sdl2_ctx->layer_data[i]) {
            SDL_DestroyTexture(sdl2_ctx->layer_data[i]);
        }
    }

    TTF_CloseFont(sysfont);

    TTF_Quit();

    SDL_DestroyRenderer(sdl2_ctx->renderer);

    SDL_DestroyWindow(sdl2_ctx->window);

    free(sdl2_ctx);

    SDL_Quit();
}

float lerp(float a, float b, float f)
{
    return a * (1.0 - f) + (b * f);
}

void vui_sdl2_set_render_color(SDL_Renderer *renderer, vui_color_t color)
{
    SDL_SetRenderDrawColor(
        renderer,
        color.r * 0xFF,
        color.g * 0xFF,
        color.b * 0xFF,
        color.a * 0xFF
    );
}

void vui_sdl2_draw_button(vui_sdl2_context_t *sdl2_ctx, vui_button_t *btn)
{
    SDL_SetRenderDrawColor(sdl2_ctx->renderer, 0, 0, 0, 0);
    SDL_RenderClear(sdl2_ctx->renderer);

    SDL_Color text_color;
    TTF_Font *text_font = sysfont;

    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = btn->w;
    rect.h = btn->h;

    switch (btn->style) {
    case VUI_BUTTON_STYLE_CORNER:
        text_font = sysfont_mini;
    case VUI_BUTTON_STYLE_BUTTON:
    case VUI_BUTTON_STYLE_LIST:
        static const float button_x_gradient_positions[] = {
            0.0f,
            0.1f,
            0.9f,
            1.0f,
        };
        static const uint8_t button_x_gradient_colors[] = {
            0xE8,
            0xFF,
            0xFF,
            0xE8,
        };
        static const float button_y_gradient_positions[] = {
            0.0f,
            0.1f,
            0.66f,
            0.875f,
            1.0f
        };
        static const uint8_t button_y_gradient_colors[] = {
            0xE8,
            0xFF,
            0xF8,
            0xE0,
            0xF0,
        };

        const int btn_radius = rect.h * 2 / 10;
        for (int y = 0; y < rect.h; y++) {
            int coord = rect.y + y;

            // Determine color
            float l = 0;
            {
                float yf = y / (float) rect.h;
                for (size_t j = 1; j < sizeof(button_y_gradient_positions)/sizeof(float); j++) {
                    float min = button_y_gradient_positions[j - 1];
                    float max = button_y_gradient_positions[j];
                    if (yf >= min && yf < max) {
                        yf -= min;
                        yf /= max - min;

                        uint8_t minc = button_y_gradient_colors[j - 1];
                        uint8_t maxc = button_y_gradient_colors[j];
                        l = lerp(minc, maxc, yf);
                        break;
                    }
                }
            }

            // Handle rounded corners
            float x_inset = 0;
            {
                if (y < btn_radius) {
                    int yb = y - btn_radius;
                    x_inset = btn_radius - sqrtf(btn_radius*btn_radius - yb*yb);
                } else {
                    if (y >= rect.h - btn_radius) {
                        int yb = (rect.h - y) - btn_radius;
                        x_inset = btn_radius - sqrtf(btn_radius*btn_radius - yb*yb);
                    }
                }
            }

            SDL_SetRenderDrawColor(sdl2_ctx->renderer, l, l, l, 0xFF);
            SDL_RenderDrawLineF(sdl2_ctx->renderer, rect.x + x_inset, coord, rect.x + rect.w - x_inset, coord);
        }

        text_color.r = text_color.g = text_color.b = 0x40;
        text_color.a = 0xFF;
        break;
    /*case VUI_BUTTON_STYLE_CORNER:
        text_font = sysfont_mini;
        break;
    case VUI_BUTTON_STYLE_LIST:
        if (list_item_count % 2 == 0) {
            SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
        } else {
            SDL_SetRenderDrawColor(renderer, 0xF0, 0xF0, 0xF0, 0xFF);
        }
        list_item_count++;

        SDL_RenderFillRect(renderer, &rect);

        text_color.r = text_color.g = text_color.b = 0;
        text_color.a = 0xFF;
        break;*/
    case VUI_BUTTON_STYLE_NONE:
        // No background
        text_color.r = text_color.g = text_color.b = text_color.a = 0xFF;
        break;
    }
    
    // Draw text
    if (btn->text[0]) {
        SDL_Surface *surface = TTF_RenderUTF8_Blended_Wrapped(text_font, btn->text, text_color, rect.w);
        SDL_Texture *texture = SDL_CreateTextureFromSurface(sdl2_ctx->renderer, surface);
        
        SDL_Rect text_rect;
        text_rect.w = surface->w;
        text_rect.h = surface->h;
        text_rect.x = rect.x + rect.w / 2 - text_rect.w / 2;
        text_rect.y = rect.y + rect.h / 2 - text_rect.h / 2;

        SDL_RenderCopy(sdl2_ctx->renderer, texture, NULL, &text_rect);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
    }
}

void vui_sdl2_draw_background(vui_context_t *ctx, SDL_Renderer *renderer)
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

void vui_draw_sdl2(vui_context_t *ctx, SDL_Renderer *renderer)
{
    vui_sdl2_context_t *sdl2_ctx = (vui_sdl2_context_t *) ctx->platform_data;

    for (int layer = 0; layer < ctx->layers; layer++) {
        if (!sdl2_ctx->layer_data[layer]) {
            // Create a new layer here
            sdl2_ctx->layer_data[layer] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, ctx->screen_width, ctx->screen_height);
            SDL_SetTextureBlendMode(sdl2_ctx->layer_data[layer], SDL_BLENDMODE_BLEND);
        }
        SDL_SetRenderTarget(renderer, sdl2_ctx->layer_data[layer]);

        if (layer == 0) {
            // Draw background
            if (!sdl2_ctx->background) {
                sdl2_ctx->background = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, ctx->screen_width, ctx->screen_height);
                SDL_Texture *old_target = SDL_GetRenderTarget(renderer);
                SDL_SetRenderTarget(renderer, sdl2_ctx->background);
                vui_sdl2_draw_background(ctx, renderer);
                SDL_SetRenderTarget(renderer, old_target);
            }
            SDL_RenderCopy(renderer, sdl2_ctx->background, 0, 0);
        } else {
            vui_sdl2_set_render_color(renderer, ctx->layer_color[layer]);
            SDL_RenderClear(renderer);
        }
    }
    
    // Draw rects
    for (int i = 0; i < ctx->rect_count; i++) {
        vui_rect_priv_t *rect = &ctx->rects[i];

        SDL_SetRenderTarget(renderer, sdl2_ctx->layer_data[rect->layer]);

        SDL_Rect sr;
        sr.x = rect->x;
        sr.y = rect->y;
        sr.w = rect->w;
        sr.h = rect->h;
        vui_sdl2_set_render_color(renderer, rect->color);
        SDL_RenderFillRect(renderer, &sr);
    }

    // Draw labels
    for (int i = 0; i < ctx->label_count; i++) {
        vui_label_t *lbl = &ctx->labels[i];

        SDL_SetRenderTarget(renderer, sdl2_ctx->layer_data[lbl->layer]);

        SDL_Color c;
        c.r = lbl->color.r * 0xFF;
        c.g = lbl->color.g * 0xFF;
        c.b = lbl->color.b * 0xFF;
        c.a = lbl->color.a * 0xFF;

        SDL_Surface *surface = TTF_RenderText_Blended_Wrapped(sysfont, lbl->text, c, lbl->w);

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

    // Draw buttons
    for (int i = 0; i < ctx->button_count; i++) {
        vui_button_t *btn = &ctx->buttons[i];

        SDL_SetRenderTarget(renderer, sdl2_ctx->layer_data[btn->layer]);

        // Check if button needs to be redrawn
        vui_sdl2_cached_texture_t *btn_tex = &sdl2_ctx->button_cache[i];
        if (!btn_tex->texture || btn_tex->w != btn->w || btn_tex->h != btn->h || strcmp(btn_tex->text, btn->text)) {
            // Must re-draw texture
            if (btn_tex->texture && (btn_tex->w != btn->w || btn_tex->h != btn->h)) {
                SDL_DestroyTexture(btn_tex->texture);
                btn_tex->texture = 0;
            }

            if (!btn_tex->texture) {
                btn_tex->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, btn->w, btn->h);
                SDL_SetTextureBlendMode(btn_tex->texture, SDL_BLENDMODE_BLEND);
            }

            SDL_Texture *old_target = SDL_GetRenderTarget(renderer);
            SDL_SetRenderTarget(renderer, btn_tex->texture);
            vui_sdl2_draw_button(sdl2_ctx, btn);
            SDL_SetRenderTarget(renderer, old_target);

            btn_tex->w = btn->w;
            btn_tex->h = btn->h;
            strcpy(btn_tex->text, btn->text);
        }

        // Draw cached texture onto screen
        SDL_Rect rect;
        rect.x = btn->sx;
        rect.y = btn->sy;
        rect.w = btn->sw;
        rect.h = btn->sh;
        
        SDL_RenderCopy(renderer, btn_tex->texture, 0, &rect);
    }
}

int vui_update_sdl2(vui_context_t *vui)
{
    vui_sdl2_context_t *sdl2_ctx = (vui_sdl2_context_t *) vui->platform_data;

    SDL_Rect *dst_rect = &sdl2_ctx->dst_rect;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            return 0;
        } else if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) {
            if (dst_rect->w > 0 && dst_rect->h > 0) {
                int tr_x, tr_y;
                tr_x = (ev.button.x - dst_rect->x) * vui->screen_width / dst_rect->w;
                tr_y = (ev.button.y - dst_rect->y) * vui->screen_height / dst_rect->h;
                if (ev.type == SDL_MOUSEBUTTONDOWN)
                    vui_process_mousedown(vui, tr_x, tr_y);
                else
                    vui_process_mouseup(vui, tr_x, tr_y);
            }
        }
    }

    vui_update(vui);

    // Draw vui to a custom texture
    vui_draw_sdl2(vui, sdl2_ctx->renderer);

    // Calculate our destination rectangle
    int win_w, win_h;
    SDL_GetWindowSize(sdl2_ctx->window, &win_w, &win_h);
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
    SDL_SetRenderTarget(sdl2_ctx->renderer, NULL);
    SDL_SetRenderDrawColor(sdl2_ctx->renderer, 0, 0, 0, 0);
    SDL_RenderClear(sdl2_ctx->renderer);
    float layer_opacity = 1.0f;
    for (int i = 0; i < vui->layers; i++) {
        layer_opacity *= vui->layer_opacity[i];
        SDL_SetTextureAlphaMod(sdl2_ctx->layer_data[i], layer_opacity* 0xFF);
        SDL_RenderCopy(sdl2_ctx->renderer, sdl2_ctx->layer_data[i], NULL, dst_rect);
    }

    // Flip surfaces
    SDL_RenderPresent(sdl2_ctx->renderer);

    return 1;
}