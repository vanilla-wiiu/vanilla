#include "Config.h"
#include <string.h>

static char gBackendIP[64];

void Config_Init(void)
{
    strcpy(gBackendIP, "");
}

void Config_SetBackendIP(const char *ip)
{
    strncpy(gBackendIP, ip, sizeof(gBackendIP) - 1);
}

const char *Config_GetBackendIP(void)
{
    return gBackendIP;
}
