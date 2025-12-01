#ifndef VPI_UI_DRM_H
#define VPI_UI_DRM_H

#include <SDL2/SDL.h>
#include <libavutil/frame.h>

typedef struct vanilla_drm_ctx_t vanilla_drm_ctx_t;

int vui_sdl_drm_initialize(vanilla_drm_ctx_t **ctx, SDL_Window *window);
int vui_sdl_drm_present(vanilla_drm_ctx_t *ctx, AVFrame *frame);
int vui_sdl_drm_free(vanilla_drm_ctx_t **ctx);

#endif // VPI_UI_DRM_H
