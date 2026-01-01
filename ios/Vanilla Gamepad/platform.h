#pragma once

#if defined(__linux__)
    #define PLATFORM_LINUX 1
#else
    #define PLATFORM_LINUX 0
#endif

#if defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE
        #define PLATFORM_IOS 1
    #else
        #define PLATFORM_IOS 0
    #endif
#else
    #define PLATFORM_IOS 0
#endif
