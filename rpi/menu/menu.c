#include "menu.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "menu_main.h"

void vpi_menu_init(vui_context_t *vui)
{
    // Start with main menu
    vpi_menu_main(vui, 0);
}