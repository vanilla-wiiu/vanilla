#ifndef VANILLA_PI_UI_UTIL_H
#define VANILLA_PI_UI_UTIL_H

#include <stddef.h>
#include <stdint.h>

void vui_strcpy(char *dst, const char *src);
size_t vui_utf8_cp_len(const char *s);
char *vui_utf8_advance(char *s);

static inline int intmin(int a, int b) { return a < b ? a : b; }
static inline int intmax(int a, int b) { return a > b ? a : b; }
static inline int64_t int64min(int64_t a, int64_t b) { return a < b ? a : b; }
static inline int64_t int64max(int64_t a, int64_t b) { return a > b ? a : b; }

#endif // VANILLA_PI_UI_UTIL_H