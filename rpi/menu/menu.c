#include "menu.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "menu_main.h"

void vanilla_menu_init(vui_context_t *vui)
{
    // Start with main menu
    vanilla_menu_main(vui, 0);
}