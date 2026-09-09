#ifndef VANILLA_UI_SDL_VIDEOTOOLBOX_H
#define VANILLA_UI_SDL_VIDEOTOOLBOX_H

#include <SDL2/SDL.h>
#include <libavutil/frame.h>

typedef struct vui_sdl_videotoolbox_context vui_sdl_videotoolbox_context_t;

vui_sdl_videotoolbox_context_t *vui_sdl_videotoolbox_create(SDL_Renderer *renderer);
void vui_sdl_videotoolbox_destroy(vui_sdl_videotoolbox_context_t **context);
int vui_sdl_videotoolbox_render(vui_sdl_videotoolbox_context_t *context,
                                SDL_Renderer *renderer,
                                const AVFrame *frame);

#endif // VANILLA_UI_SDL_VIDEOTOOLBOX_H
