#ifndef VANILLA_PI_UI_SDL_H
#define VANILLA_PI_UI_SDL_H

#include "ui.h"

/**
 * System-related functions
 */
int vui_init_sdl(vui_context_t *ctx, int fullscreen);
void vui_close_sdl(vui_context_t *ctx);

// Return whether direct VideoToolbox to display path is available (i.e. if Metal is being used)
int vui_sdl_videotoolbox_available(vui_context_t *ctx);

/**
 * Main loop
 * 
 * Intended to be called from a semi-infinite loop.
 * 
 * Returns 1 if the loop should continue, 0 if the loop should exit
 */
int vui_update_sdl(vui_context_t *ctx);

#endif // VANILLA_PI_UI_SDL_H
