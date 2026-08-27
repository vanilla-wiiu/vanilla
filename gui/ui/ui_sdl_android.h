#ifndef VANILLA_UI_SDL_ANDROID_H
#define VANILLA_UI_SDL_ANDROID_H

#include <SDL2/SDL.h>

int vui_sdl_android_create_video_texture(SDL_Renderer *renderer, int width,
                                         int height, SDL_Texture **texture);
void *vui_sdl_android_get_video_surface(void);
int vui_sdl_android_update_video_texture(float transform[16]);
void vui_sdl_android_destroy_video_texture(void);
int vui_sdl_android_set_brightness(float brightness);

#endif
