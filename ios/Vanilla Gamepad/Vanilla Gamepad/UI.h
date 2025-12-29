#pragma once
#include <SDL3/SDL.h>
#include <stdbool.h>

bool UI_Init(SDL_Window *window, SDL_Renderer *renderer);
void UI_HandleEvent(const SDL_Event *e);
void UI_Render(void);
void UI_Quit(void);

const char *UI_GetIP(void);
