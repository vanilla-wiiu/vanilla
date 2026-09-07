#include "platform.h"

#include <SDL_filesystem.h>
#include <SDL_log.h>
#include <stdio.h>

#include "ui/ui_util.h"

static char pref_path[4096] = {0};

void vpi_config_filename(char *out, size_t out_size)
{
    vpi_get_data_filename(out, out_size, "config.xml", NULL);
}

void vpi_get_data_filename(char *out, size_t out_size, const char *filename, const char *preferred_dir)
{
    if (preferred_dir && preferred_dir[0]) {
        snprintf(out, out_size, "%s/%s", preferred_dir, filename);
    } else {
        if (!pref_path[0]) {
            char *s = SDL_GetPrefPath("", "Vanilla");
            vui_strncpy(pref_path, s, sizeof(pref_path));
            SDL_free(s);
        }
        snprintf(out, out_size, "%s%s", pref_path, filename);
    }
}

void vpilog_va(const char *fmt, va_list va)
{
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, va);
}

void vpilog(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vpilog_va(fmt, va);
    va_end(va);
}

void vpi_asset_filename(char *buf, size_t size, const char *type, const char *filename)
{
#if defined(ANDROID)
    snprintf(buf, size, "%s/%s", type, filename);
#elif defined(_WIN32) || defined(__APPLE__)
    char *base_path = SDL_GetBasePath();
    snprintf(buf, size, "%s/assets/%s/%s", base_path, type, filename);
    SDL_free(base_path);
#else
    char *base_path = SDL_GetBasePath();
    snprintf(buf, size, "%s/../share/vanilla/assets/%s/%s", base_path, type, filename);
    SDL_free(base_path);
#endif
}
