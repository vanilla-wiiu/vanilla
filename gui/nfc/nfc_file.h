#ifndef VANILLA_PI_NFC_NFC_FILE_H
#define VANILLA_PI_NFC_NFC_FILE_H

#include "vanilla.h"

void nfc_file_touch_tag(const char *path);

extern VanillaNfcBackend nfc_file_backend;

#endif // VANILLA_PI_NFC_NFC_FILE_H
