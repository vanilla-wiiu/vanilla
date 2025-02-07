#include "ui_sdl.h"

#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include "ui_priv.h"
#include "ui_util.h"

static TTF_Font *sysfont = 0;
static TTF_Font *sysfont_mini = 0;

typedef struct {
    SDL_Texture *texture;
    int w;
    int h;
    char text[MAX_BUTTON_TEXT];
    char icon[MAX_BUTTON_TEXT];
} vui_sdl_cached_texture_t;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Rect dst_rect;
    SDL_Texture *background;
    SDL_Texture *layer_data[MAX_BUTTON_COUNT];
    vui_sdl_cached_texture_t button_cache[MAX_BUTTON_COUNT];
    vui_sdl_cached_texture_t image_cache[MAX_BUTTON_COUNT];
} vui_sdl_context_t;

int vui_init_sdl(vui_context_t *ctx, int fullscreen)
{
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "Failed to initialize SDL\n");
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
        return -1;
    }

    sdl_ctx->renderer = SDL_CreateRenderer(sdl_ctx->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdl_ctx->renderer) {
        return -1;
    }

    if (TTF_Init()) {
        return -1;
    }

    const char *font_filename = "/home/matt/src/vanilla/rpi/font/system.ttf";

    sysfont = TTF_OpenFont(font_filename, 36);
    if (!sysfont) {
        return -1;
    }

    sysfont_mini = TTF_OpenFont(font_filename, 24);
    if (!sysfont_mini) {
        return -1;
    }

    TTF_SetFontWrappedAlign(sysfont, TTF_WRAPPED_ALIGN_CENTER);
    TTF_SetFontWrappedAlign(sysfont_mini, TTF_WRAPPED_ALIGN_CENTER);

    sdl_ctx->background = 0;
    memset(sdl_ctx->layer_data, 0, sizeof(sdl_ctx->layer_data));
    memset(sdl_ctx->button_cache, 0, sizeof(sdl_ctx->button_cache));
    memset(sdl_ctx->image_cache, 0, sizeof(sdl_ctx->image_cache));

    return 0;
}

void vui_close_sdl(vui_context_t *ctx)
{
    vui_sdl_context_t *sdl_ctx = (vui_sdl_context_t *) ctx->platform_data;
    if (!sdl_ctx) {
        return;
    }

    for (int i = 0; i < MAX_BUTTON_COUNT; i++) {
        if (sdl_ctx->layer_data[i]) {
            SDL_DestroyTexture(sdl_ctx->layer_data[i]);
        }
    }

    TTF_CloseFont(sysfont_mini);
    TTF_CloseFont(sysfont);

    TTF_Quit();

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
    snprintf(buf, sizeof(buf), "/home/matt/src/vanilla/rpi/tex/%s", filename);
    return IMG_LoadTexture(renderer, buf);
}

void vui_sdl_draw_button(vui_sdl_context_t *sdl_ctx, vui_button_t *btn)
{
    SDL_SetRenderDrawColor(sdl_ctx->renderer, 0, 0, 0, 0);
    SDL_RenderClear(sdl_ctx->renderer);

    SDL_Color text_color;
    TTF_Font *text_font = btn->style != VUI_BUTTON_STYLE_CORNER ? sysfont : sysfont_mini;

    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = btn->w;
    rect.h = btn->h;

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
        if (btn->style != VUI_BUTTON_STYLE_CORNER) {
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

        SDL_SetRenderDrawColor(sdl_ctx->renderer, l, l, l, 0xFF);
        SDL_RenderDrawLineF(sdl_ctx->renderer, rect.x + x_inset, coord, rect.x + rect.w - x_inset, coord);
    }

    text_color.r = text_color.g = text_color.b = 0x40;
    text_color.a = 0xFF;

    // Load icon if exists
    int icon_x;
    int icon_size = rect.h * 3 / 4;
    SDL_Texture *icon = 0;
    if (btn->icon[0]) {
        icon = vui_sdl_load_texture(sdl_ctx->renderer, btn->icon);
    }
    
    // Draw text
    if (btn->text[0]) {
        SDL_Surface *surface = TTF_RenderUTF8_Blended_Wrapped(text_font, btn->text, text_color, rect.w);
        SDL_Texture *texture = SDL_CreateTextureFromSurface(sdl_ctx->renderer, surface);

        SDL_Rect text_rect;
        
        text_rect.w = surface->w;
        text_rect.h = surface->h;

        if (icon) {
            const int btn_padding = 5;
            icon_x = rect.x + rect.w / 2 - (text_rect.w + icon_size) / 2;
            text_rect.x = icon_x + icon_size + btn_padding;
        } else {
            text_rect.x = rect.x + rect.w / 2 - text_rect.w / 2;
        }
        
        text_rect.y = rect.y + rect.h / 2 - text_rect.h / 2;

        SDL_RenderCopy(sdl_ctx->renderer, texture, NULL, &text_rect);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
    } else {
        icon_x = rect.x + rect.w/2 - icon_size/2;
    }

    // Draw icon
    if (icon) {
        SDL_Rect icon_rect;
        icon_rect.w = icon_rect.h = icon_size;
        icon_rect.x = icon_x;
        icon_rect.y = rect.y + rect.h/2 - icon_rect.h/2;

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

        if (layer == 0) {
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
            // vui_sdl_set_render_color(renderer, ctx->layer_color[layer]);
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
        SDL_RenderFillRect(renderer, &sr);
    }

    // Draw labels
    for (int i = 0; i < ctx->label_count; i++) {
        vui_label_t *lbl = &ctx->labels[i];

        SDL_SetRenderTarget(renderer, sdl_ctx->layer_data[lbl->layer]);

        SDL_Color c;
        c.r = lbl->color.r * 0xFF;
        c.g = lbl->color.g * 0xFF;
        c.b = lbl->color.b * 0xFF;
        c.a = lbl->color.a * 0xFF;

        SDL_Surface *surface = TTF_RenderUTF8_Blended_Wrapped(lbl->size == VUI_FONT_SIZE_SMALL ? sysfont_mini : sysfont, lbl->text, c, lbl->w);

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

        SDL_SetRenderTarget(renderer, sdl_ctx->layer_data[btn->layer]);

        // Check if button needs to be redrawn
        vui_sdl_cached_texture_t *btn_tex = &sdl_ctx->button_cache[i];
        if (!btn_tex->texture || btn_tex->w != btn->w || btn_tex->h != btn->h || strcmp(btn_tex->text, btn->text) || strcmp(btn_tex->icon, btn->icon)) {
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
            vui_sdl_draw_button(sdl_ctx, btn);
            SDL_SetRenderTarget(renderer, old_target);

            btn_tex->w = btn->w;
            btn_tex->h = btn->h;
            strcpy(btn_tex->text, btn->text);
            strcpy(btn_tex->icon, btn->icon);
        }

        // Draw cached texture onto screen
        SDL_Rect rect;
        rect.x = btn->sx;
        rect.y = btn->sy;
        rect.w = btn->sw;
        rect.h = btn->sh;
        
        SDL_RenderCopy(renderer, btn_tex->texture, 0, &rect);
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

int vui_update_sdl(vui_context_t *vui)
{
    vui_sdl_context_t *sdl_ctx = (vui_sdl_context_t *) vui->platform_data;

    SDL_Rect *dst_rect = &sdl_ctx->dst_rect;

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
    vui_draw_sdl(vui, sdl_ctx->renderer);

    // Flatten layers
    for (int i = vui->layers - 1; i > 0; i--) {
        SDL_Texture *bg = sdl_ctx->layer_data[i-1];
        SDL_Texture *fg = sdl_ctx->layer_data[i];

        SDL_SetRenderTarget(sdl_ctx->renderer, bg);
        SDL_SetTextureColorMod(fg, vui->layer_opacity[i] * 0xFF, vui->layer_opacity[i] * 0xFF, vui->layer_opacity[i] * 0xFF);
        SDL_SetTextureAlphaMod(fg, vui->layer_opacity[i] * 0xFF);
        SDL_RenderCopy(sdl_ctx->renderer, fg, NULL, NULL);
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
    SDL_SetRenderTarget(sdl_ctx->renderer, NULL);
    SDL_SetRenderDrawColor(sdl_ctx->renderer, 0, 0, 0, 0);
    SDL_RenderClear(sdl_ctx->renderer);
    SDL_RenderCopy(sdl_ctx->renderer, sdl_ctx->layer_data[0], NULL, dst_rect);

    // Flip surfaces
    SDL_RenderPresent(sdl_ctx->renderer);

    return 1;
}