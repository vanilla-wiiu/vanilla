#include "UI.h"
#include <SDL3_ttf/SDL_ttf.h>
#include <string.h>

static SDL_Window   *gWindow   = NULL;
static SDL_Renderer *gRenderer = NULL;
static TTF_Font     *gFont     = NULL;

static char ipBuffer[64] = "";
static int ipLen = 0;

static SDL_FRect ipBox = {200, 240, 500, 70};

bool UI_Init(SDL_Window *window, SDL_Renderer *renderer)
{
    gWindow   = window;
    gRenderer = renderer;

    if (TTF_Init() < 0) {
        SDL_Log("TTF_Init failed: %s", SDL_GetError());
        return false;
    }

    gFont = TTF_OpenFont("contm.ttf", 32);
    if (!gFont) {
        SDL_Log("Font load failed: %s", SDL_GetError());
        return false;
    }

    SDL_StartTextInput(gWindow);
    SDL_Log("UI initialized");
    return true;
}

void UI_HandleEvent(const SDL_Event *e)
{
    if (e->type == SDL_EVENT_TEXT_INPUT) {
        if (ipLen < (int)sizeof(ipBuffer) - 1) {
            strcat(ipBuffer, e->text.text);
            ipLen = (int)strlen(ipBuffer);
        }
    }
    else if (e->type == SDL_EVENT_KEY_DOWN) {
        if (e->key.key == SDLK_BACKSPACE && ipLen > 0) {
            ipBuffer[--ipLen] = '\0';
        }
    }
}

static void DrawText(const char *text, float x, float y)
{
    SDL_Color white = {255, 255, 255, 255};

    SDL_Surface *surface =
        TTF_RenderText_Blended(gFont, text, strlen(text), white);
    if (!surface) return;

    SDL_Texture *tex =
        SDL_CreateTextureFromSurface(gRenderer, surface);
    if (!tex) {
        SDL_DestroySurface(surface);
        return;
    }

    SDL_FRect dst = {
        x,
        y,
        (float)surface->w,
        (float)surface->h
    };

    SDL_RenderTexture(gRenderer, tex, NULL, &dst);

    SDL_DestroyTexture(tex);
    SDL_DestroySurface(surface);
}

void UI_Render(void)
{
    SDL_SetRenderDrawColor(gRenderer, 120, 190, 255, 255);
    SDL_RenderClear(gRenderer);

    SDL_SetRenderDrawColor(gRenderer, 255, 255, 255, 255);
    SDL_RenderFillRect(gRenderer, &ipBox);

    SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
    SDL_RenderRect(gRenderer, &ipBox);

    DrawText("Backend IP:", 200, 190);

    DrawText(ipBuffer[0] ? ipBuffer : "Enter IP address",
             ipBox.x + 15,
             ipBox.y + 20);

    SDL_RenderPresent(gRenderer);
}

void UI_Quit(void)
{
    SDL_StopTextInput(gWindow);

    if (gFont)
        TTF_CloseFont(gFont);

    TTF_Quit();
}

const char *UI_GetIP(void)
{
    return ipBuffer;
}
