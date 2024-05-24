#include "status.h"

#include <stdio.h>
#include <stdarg.h>

#include "vanilla.h"

static const char *VANILLA_STATUS_STRINGS[] = {
    "SUCCESS",
    "ERROR",
    "READY",
    "INFO",
    "UNKNOWN_COMMAND",
    "INVALID_ARGUMENT"
};

void print_status(int errno)
{
    vanilla_log("%s", VANILLA_STATUS_STRINGS[-errno]);
}

void print_info(const char *errstr, ...)
{
    va_list args;
    va_start(args, errstr);

    vanilla_log_no_newline(VANILLA_STATUS_STRINGS[-VANILLA_INFO]);
    vanilla_log_no_newline(" ");
    vanilla_log_no_newline_va(errstr, args);
    vanilla_log_no_newline("\n");

    va_end(args);
}
