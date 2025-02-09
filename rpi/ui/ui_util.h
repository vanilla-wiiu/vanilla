#ifndef VANILLA_PI_UI_UTIL_H
#define VANILLA_PI_UI_UTIL_H

#include <stdint.h>

void vui_strcpy(char *dst, const char *src);

static inline int intmin(int a, int b) { return a < b ? a : b; }
static inline int intmax(int a, int b) { return a > b ? a : b; }
static inline int64_t int64min(int64_t a, int64_t b) { return a < b ? a : b; }
static inline int64_t int64max(int64_t a, int64_t b) { return a > b ? a : b; }

#endif // VANILLA_PI_UI_UTIL_H