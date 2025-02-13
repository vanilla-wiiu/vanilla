#ifndef VANILLA_PI_CONFIG_H
#define VANILLA_PI_CONFIG_

#include <stdint.h>
#include <vanilla.h>

#define VPI_CONSOLE_MAX_NAME 256

typedef struct {
    char name[VPI_CONSOLE_MAX_NAME];
    vanilla_bssid_t bssid;
    vanilla_psk_t psk;
} vpi_console_entry_t;

typedef struct {
    uint8_t connected_console_count;
    vpi_console_entry_t *connected_console_entries;
    uint32_t server_address;
} vpi_config_t;

extern vpi_config_t vpi_config;

void vpi_config_init();
void vpi_config_free();
void vpi_config_add_console(vpi_console_entry_t *entry);
void vpi_config_remove_console(uint8_t index);

#endif // VANILLA_PI_CONFIG_H