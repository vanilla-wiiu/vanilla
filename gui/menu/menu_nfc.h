#ifndef VANILLA_PI_MENU_NFC_H
#define VANILLA_PI_MENU_NFC_H

#include "ui/ui.h"

extern const VanillaNfcBackend *nfc_backends[];
extern size_t nfc_num_backends;

void vpi_menu_nfc(vui_context_t *vui, void *v);

#endif // VANILLA_PI_MENU_NFC_H
