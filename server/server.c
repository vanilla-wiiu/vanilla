#include "server.h"

#include "gamepad.h"
#include "status.h"
#include "sync.h"
#include "util.h"

int vanilla_sync_with_console(const char *wireless_interface, uint16_t code)
{
    return sync_with_console_internal(wireless_interface, code);
}

int vanilla_connect_to_console(const char *wireless_interface)
{
    return connect_as_gamepad_internal(wireless_interface);
}
