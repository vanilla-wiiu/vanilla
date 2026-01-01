#include "platform.h"

#if PLATFORM_IOS

int vanilla_get_sysinfo(void *info)
{
    // Not available on iOS
    return -1;
}

#endif
