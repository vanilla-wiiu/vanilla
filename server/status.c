#include "status.h"

#include <stdio.h>
#include <stdarg.h>

static const char *VANILLA_STATUS_STRINGS[] = {
    "SUCCESS",
    "ERROR",
    "READY",
    "INFO",
    "UNKNOWN_COMMAND",
    "INVALID_ARGUMENT"
};

//static const char *VANILL

void print_status(int errno)
{
    printf("%s\n", VANILLA_STATUS_STRINGS[-errno]);
}

void print_info(const char *errstr, ...)
{
    va_list args;
    va_start(args, errstr);

    char buf[16384];
    vsnprintf(buf, sizeof(buf), errstr, args);
    
    printf("%s %s\n", VANILLA_STATUS_STRINGS[-VANILLA_INFO], buf);

    va_end(args);
}
