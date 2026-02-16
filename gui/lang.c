#include <string.h>
#include "lang.h"
#include "langs/english_en/english_en.h"
#include "langs/french_fr/french_fr.h"
static char *langs_order[2] = {
    "en",
    "fr",
};
static const int lang_c = sizeof(langs_order) / sizeof(langs_order[0]);
static const char *lang_str[__VPI_LANG_T_COUNT];
char lang_history[3];

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
    memcpy(lang_history, curr_lang, sizeof(lang_history));
    return 1;
}

char *switch_lang(const char *curr_lang){
    for(int i = 0; i < lang_c; i++){
        if(strcmp(langs_order[i], curr_lang) == 0) {
            int next = (i + 1) % lang_c;
            lang_choice(langs_order[next]);
            return langs_order[next];
        }
    }
    return NULL;
}

const char *get_lang(){
    return lang_history;
}

const char *lang(vpi_lang_t id)
{
    return lang_str[id];
}
