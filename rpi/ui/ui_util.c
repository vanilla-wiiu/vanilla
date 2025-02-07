#include "ui_util.h"

#include <string.h>

#include "ui_priv.h"

void vui_strcpy(char *dst, const char *src)
{
    if (src)
        strncpy(dst, src, MAX_BUTTON_TEXT);
    else
        dst[0] = 0;
    dst[MAX_BUTTON_TEXT-1] = 0;
}