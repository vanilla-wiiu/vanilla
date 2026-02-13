#include <string.h>
#include "lang.h"
#include "langs/english_en/english_en.h"
#include "langs/french_fr/french_fr.h"

static const char *lang_str[__VPI_LANG_T_COUNT];

int lang_choice(char *curr_lang){
    if (strcmp(curr_lang, "en") == 0) {
        memcpy(lang_str, lang_str_en, sizeof(lang_str));
    }
    else if (strcmp(curr_lang, "fr") == 0) {
        memcpy(lang_str, lang_str_fr, sizeof(lang_str));
    }
    else {
        return 0;
    }
    return 1;
}


const char *lang(vpi_lang_t id)
{
    return lang_str[id];
}
