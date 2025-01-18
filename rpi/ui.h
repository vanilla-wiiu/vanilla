#ifndef VANILLA_PI_UI_H
#define VANILLA_PI_UI_H

typedef struct vui_context_t vui_context_t;

vui_context_t *vui_alloc();
int vui_reset(vui_context_t *ctx);
int vui_draw_button(vui_context_t *ctx, int x, int y, int w, int h, const char *text);
int vui_process_click(vui_context_t *ctx, int x, int y);
void vui_free(vui_context_t *ctx);

#endif // VANILLA_PI_UI_H