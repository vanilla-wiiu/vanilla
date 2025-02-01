#ifndef VANILLA_PI_UI_SDL_H
#define VANILLA_PI_UI_SDL_H

#include <SDL2/SDL.h>

#include "ui.h"


/**
 * System-related functions
 */
int vui_init_sdl2();
void vui_close_sdl2();

void vui_draw_sdl2(vui_context_t *ctx, SDL_Renderer *renderer);

#endif // VANILLA_PI_UI_SDL_H