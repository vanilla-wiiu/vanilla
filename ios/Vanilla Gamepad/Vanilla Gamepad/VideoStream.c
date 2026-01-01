#include "VideoStream.h"
#include "pipe/def.h"
#include "pipe/pipe_client.h"

static SDL_Texture *tex;
static int fb_w = 1280, fb_h = 720;

void VideoStream_Init(SDL_Renderer *r)
{
    tex = SDL_CreateTexture(
        r,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        fb_w, fb_h);
}

void VideoStream_Update(void)
{
    void *pixels;
    int pitch;

    SDL_LockTexture(tex, NULL, &pixels, &pitch);

    // EXACT SAME BUFFER YOUR ANDROID CLIENT FILLS
    Pipe_ReadFramebuffer(pixels, pitch);

    SDL_UnlockTexture(tex);
}

void VideoStream_Render(void)
{
    SDL_RenderTexture(SDL_GetRenderer(tex), tex, NULL, NULL);
}
