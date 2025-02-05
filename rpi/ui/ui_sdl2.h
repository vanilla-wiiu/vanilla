#ifndef VANILLA_PI_UI_SDL_H
#define VANILLA_PI_UI_SDL_H

#include <SDL2/SDL.h>

#include "ui.h"

/**
 * System-related functions
 */
int vui_init_sdl2(vui_context_t *ctx);
void vui_close_sdl2(vui_context_t *ctx);

/**
 * Main loop
 * 
 * Intended to be called from a semi-infinite loop.
 * 
 * Returns 1 if the loop should continue, 0 if the loop should exit
 */
int vui_update_sdl2(vui_context_t *ctx);

#endif // VANILLA_PI_UI_SDL_H