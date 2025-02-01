#include "ui_sdl2.h"

#include <SDL2/SDL_ttf.h>

#include "ui_priv.h"

static TTF_Font *sysfont = 0;
static TTF_Font *sysfont_mini = 0;

int vui_init_sdl2()
{
    if (TTF_Init()) {
        return -1;
    }

    const char *font_filename = "/home/matt/src/vanilla/rpi/font/system.ttf";

    sysfont = TTF_OpenFont(font_filename, 34);
    if (!sysfont) {
        return -1;
    }

    sysfont_mini = TTF_OpenFont(font_filename, 18);
    if (!sysfont_mini) {
        return -1;
    }

    return 0;
}

void vui_close_sdl2()
{
    TTF_CloseFont(sysfont);

    TTF_Quit();
}

void vui_draw_sdl2(vui_context_t *ctx, SDL_Renderer *renderer)
{
    SDL_SetRenderDrawColor(renderer, 0x0, 0x0, 0x0, 0x0);
    SDL_RenderClear(renderer);

    int list_item_count = 0;

    for (int i = 0; i < ctx->button_count; i++) {
        // Draw background
        vui_button_t *btn = &ctx->buttons[i];
        SDL_Rect rect;
        rect.x = btn->x;
        rect.y = btn->y;
        rect.w = btn->w;
        rect.h = btn->h;

        SDL_Color text_color;
        TTF_Font *text_font = sysfont;

        switch (btn->style) {
        case VUI_BUTTON_STYLE_BUTTON:
            SDL_SetRenderDrawColor(renderer, 0xFF, 0x0, 0x0, 0xFF);
            SDL_RenderFillRect(renderer, &rect);

            text_color.r = text_color.g = text_color.b = text_color.a = 0xFF;
            break;
        case VUI_BUTTON_STYLE_CORNER:
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
            break;
        case VUI_BUTTON_STYLE_NONE:
            // No background
            text_color.r = text_color.g = text_color.b = text_color.a = 0xFF;
            break;
        }

        
        // Draw text
        if (btn->text[0]) {
            SDL_Surface *surface = TTF_RenderUTF8_Blended_Wrapped(text_font, btn->text, text_color, rect.w);
            SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
            
            SDL_Rect text_rect;
            text_rect.w = surface->w;
            text_rect.h = surface->h;
            text_rect.x = rect.x + rect.w / 2 - text_rect.w / 2;
            text_rect.y = rect.y + rect.h / 2 - text_rect.h / 2;

            SDL_RenderCopy(renderer, texture, NULL, &text_rect);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
    }

    if (ctx->overlay_enabled) {
        SDL_SetRenderDrawColor(renderer, ctx->overlay_r * 0xFF, ctx->overlay_g * 0xFF, ctx->overlay_b * 0xFF, ctx->overlay_a * 0xFF);
        SDL_RenderFillRect(renderer, NULL);
    }
}