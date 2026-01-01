#define SDL_MAIN_HANDLED

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdbool.h>

#include "UI.h"

/* ------------------------------------------------------------ */
/* SDL entry point                                              */
/* ------------------------------------------------------------ */
static int VanillaMain(int argc, char *argv[])
{
    SDL_Log("VanillaMain starting");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS) < 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Vanilla Gamepad",
        1280,
        720,
        SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        SDL_Log("Window creation failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, "metal");
    if (!renderer) {
        SDL_Log("Renderer creation failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("Window + renderer created");

    int rw, rh;
    SDL_GetRenderOutputSize(renderer, &rw, &rh);
    SDL_Log("Render output size: %dx%d", rw, rh);

    /* Initialize UI */
    UI_Init(window, renderer);

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                running = false;
            }
            UI_HandleEvent(&e);
        }

        UI_Render();
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

/* ------------------------------------------------------------ */
/* iOS main — ONLY ONE UIApplication comes from SDL_RunApp      */
/* ------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    return SDL_RunApp(argc, argv, VanillaMain, NULL);
}

