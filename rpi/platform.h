#ifndef VANILLA_PI_PLATFORM_H
#define VANILLA_PI_PLATFORM_H

#include <stdarg.h>
#include <stdlib.h>

void vpi_config_filename(char *out, size_t out_size);
void vpi_get_data_filename(char *out, size_t out_size, const char *filename);

void vpilog_va(const char *fmt, va_list va);
void vpilog(const char *fmt, ...);

void vpi_asset_filename(char *buf, size_t size, const char *type, const char *filename);

void *vpi_start_process(const char **args);

#endif // VANILLA_PI_PLATFORM_H