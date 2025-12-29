#pragma once
#include <SDL3/SDL.h>

void Font_DrawText(SDL_Renderer *r, int x, int y, const char *text);
int  Font_TextWidth(const char *text);
