#include "menu_common.h"

void vpi_menu_create_background(vui_context_t *vui, int layer, vui_rect_t *bkg_rect, int *margin)
{
    int scrw, scrh;
    vui_get_screen_size(vui, &scrw, &scrh);

    int m = scrw/75;
    *margin = m;
    bkg_rect->x = m;
    bkg_rect->y = m;
    bkg_rect->w = scrw-m-m;
    bkg_rect->h = scrh-m-m;
    
    vui_rect_create(vui, bkg_rect->x, bkg_rect->y, bkg_rect->w, bkg_rect->h, scrh*2/10, vui_color_create(0, 0, 0, 0.66f), layer);
}