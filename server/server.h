#ifndef VANILLA_SERVER_H
#define VANILLA_SERVER_H

#include <stdint.h>

/**
 * Attempt to sync with the console
 */
int vanilla_sync_with_console(const char *wireless_interface, uint16_t code);

/**
 * Attempt gameplay connection with console
 */
int vanilla_connect_to_console(const char *wireless_interface);

/**
 * Determine if currently connected to the console
 */
int vanilla_is_connected();

#endif // VANILLA_SERVER_H