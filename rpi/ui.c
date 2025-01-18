#include "ui.h"

#include <stdlib.h>
#include <string.h>

#define MAX_BUTTON_COUNT 8
#define MAX_BUTTON_TEXT 16

typedef struct {
    int x;
    int y;
    int w;
    int h;
    char text[MAX_BUTTON_TEXT];
} vui_button_t;

typedef struct vui_context_t {
    vui_button_t buttons[MAX_BUTTON_COUNT];
    int button_count;
} vui_context_t;

vui_context_t *vui_alloc()
{
    return malloc(sizeof(vui_context_t));
}

void vui_free(vui_context_t *vui)
{
    vui_reset(vui);
    free(vui);
}

int vui_reset(vui_context_t *ctx)
{
    ctx->button_count = 0;
}

int vui_draw_button(vui_context_t *ctx, int x, int y, int w, int h, const char *text)
{
    if (ctx->button_count == MAX_BUTTON_COUNT) {
        return -1;
    }

    int index = ctx->button_count;

    vui_button_t *btn = &ctx->buttons[index];

    btn->x = x;
    btn->y = y;
    btn->w = w;
    btn->h = h;

    // Copy text (enforce null terminator)
    strncpy(btn->text, text, MAX_BUTTON_TEXT);
    btn->text[MAX_BUTTON_TEXT-1] = 0;

    ctx->button_count++;

    return index;
}

int vui_process_click(vui_context_t *ctx, int x, int y)
{
    for (int i = 0; i < ctx->button_count; i++) {
        vui_button_t *btn = &ctx->buttons[i];

        if (x >= btn->x && y >= btn->y && x < btn->x + btn->w && y < btn->y + btn->h) {
            return i;
        }
    }

    return -1;
}