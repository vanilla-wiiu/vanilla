#ifndef VANILLA_SERVER_H
#define VANILLA_SERVER_H

#include <stdint.h>

#define VANILLA_ERROR_SUCCESS           0
#define VANILLA_ERROR_UNKNOWN           -100
#define VANILLA_ERROR_CONFIG_FILE       -101
#define VANILLA_ERROR_BAD_FORK          -102
#define VANILLA_ERROR_TERMINATED        -103
#define VANILLA_ERROR_CANCELLED         -104

int vanilla_sync_with_console(const char *wireless_interface, uint16_t code);

#endif // VANILLA_SERVER_H