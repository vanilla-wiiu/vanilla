#include <SDL2/SDL.h>
#include <stdio.h>

#include "menu/menu.h"
#include "ui/ui.h"
#include "ui/ui_sdl2.h"

#define SCREEN_WIDTH    854
#define SCREEN_HEIGHT   480

int main()
{
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "Failed to initialize SDL\n");
        return 1;
    }

    // Initialize VUI
    if (vui_init_sdl2()) {
        fprintf(stderr, "Failed to initialize VUI\n");
        return 1;
    }

    // Initialize UI system
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    SDL_Window *window = SDL_CreateWindow("vanilla", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_RESIZABLE);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    SDL_Texture *maintex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, SCREEN_WIDTH, SCREEN_HEIGHT);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    int running = 1;

    vui_context_t *vui = vui_alloc();

    vanilla_menu_init(vui);

    SDL_Rect dst_rect = {0};

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = 0;
                goto exit_loop;
            } else if (ev.type == SDL_MOUSEBUTTONDOWN) {
                if (dst_rect.w > 0 && dst_rect.h > 0) {
                    int tr_x, tr_y;
                    tr_x = (ev.button.x - dst_rect.x) * SCREEN_WIDTH / dst_rect.w;
                    tr_y = (ev.button.y - dst_rect.y) * SCREEN_HEIGHT / dst_rect.h;
                    vui_process_click(vui, tr_x, tr_y);
                }
            }
        }

        vui_update(vui);

        // Draw vui to a custom texture
        SDL_SetRenderTarget(renderer, maintex);
        vui_draw_sdl2(vui, renderer);

        // Calculate our destination rectangle
        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        if (win_w == SCREEN_WIDTH && win_h == SCREEN_HEIGHT) {
            dst_rect.x = 0;
            dst_rect.y = 0;
            dst_rect.w = SCREEN_WIDTH;
            dst_rect.h = SCREEN_HEIGHT;
        } else if (win_w * 100 / win_h > SCREEN_WIDTH * 100 / SCREEN_HEIGHT) {
            // Window is wider than texture, scale by height
            dst_rect.h = win_h;
            dst_rect.y = 0;
            dst_rect.w = win_h * SCREEN_WIDTH / SCREEN_HEIGHT;
            dst_rect.x = win_w / 2 - dst_rect.w / 2;
        } else {
            // Window is taller than texture, scale by width
            dst_rect.w = win_w;
            dst_rect.x = 0;
            dst_rect.h = win_w * SCREEN_HEIGHT / SCREEN_WIDTH;
            dst_rect.y = win_h / 2 - dst_rect.h / 2;
        }

        // Copy texture to window
        SDL_SetRenderTarget(renderer, NULL);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, maintex, NULL, &dst_rect);

        // Flip surfaces
        SDL_RenderPresent(renderer);
    }

    vui_free(vui);

exit_loop:
    SDL_DestroyRenderer(renderer);

    SDL_DestroyWindow(window);

    vui_close_sdl2();

    SDL_Quit();
}