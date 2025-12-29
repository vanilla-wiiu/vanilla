#pragma once

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
  Initialize the video stream system.
  Must be called once after SDL_CreateRenderer().
*/
void VideoStream_Init(SDL_Renderer *renderer);

/*
  Pull the latest framebuffer from the backend
  and upload it into the SDL texture.
*/
void VideoStream_Update(void);

/*
  Render the current framebuffer to the screen.
  Call between SDL_RenderClear() and SDL_RenderPresent().
*/
void VideoStream_Render(void);

/*
  Optional cleanup (safe to call on shutdown).
*/
void VideoStream_Shutdown(void);

#ifdef __cplusplus
}
#endif
