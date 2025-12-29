#include "Font.h"

/* 6x8 bitmap font (ASCII 32–127, minimal) */
static const unsigned char font6x8[][8] = {
    /* space */ {0,0,0,0,0,0,0,0},
    /* ! */ {0x18,0x18,0x18,0x18,0x18,0,0x18,0},
    /* " */ {0x36,0x36,0x12,0,0,0,0,0},
    /* # */ {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0},
    /* $ */ {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0},
    /* % */ {0x63,0x33,0x18,0x0C,0x66,0x63,0,0},
    /* & */ {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0},
    /* ' */ {0x18,0x18,0x0C,0,0,0,0,0},
};

/* fallback for digits + dot */
static void DrawChar(SDL_Renderer *r, int x, int y, char c)
{
    if (c < ' ' || c > '~') return;

    SDL_SetRenderDrawColor(r, 20, 20, 20, 255);

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 6; col++) {
            if ((c >= '0' && c <= '9') || c == '.') {
                SDL_FRect px = { x + col*2, y + row*2, 2, 2 };
                SDL_RenderFillRect(r, &px);
            }
        }
    }
}

void Font_DrawText(SDL_Renderer *r, int x, int y, const char *text)
{
    int cx = x;
    while (*text) {
        DrawChar(r, cx, y, *text++);
        cx += 14;
    }
}

int Font_TextWidth(const char *text)
{
    int len = 0;
    while (*text++) len++;
    return len * 14;
}
