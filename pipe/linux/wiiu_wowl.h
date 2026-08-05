#ifndef VANILLA_PIPE_LINUX_WIIU_WOWL_H
#define VANILLA_PIPE_LINUX_WIIU_WOWL_H

#include "vanilla.h"

int wiiu_wowl_try_wake(const char *ifname, const vanilla_connection_t *connection);

#endif // VANILLA_PIPE_LINUX_WIIU_WOWL_H
