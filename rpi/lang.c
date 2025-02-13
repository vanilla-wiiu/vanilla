#include "lang.h"

static const char *lang_str[__VPI_LANG_T_COUNT] = {
    "Sync",
    "Connect",
    "Edit",
    "Settings",
    "OK",
    "Cancel",
    "Touch the symbols in the order they are displayed on the TV screen from left to right.",
    "If the symbols are not displayed on the TV screen, press the SYNC Button on the Wii U console.",
    "Connecting to the Wii U console...",
    "Sync Failed",
    "Vanilla needs root permission to configure the wireless interface for connection.",
    "Please enter your \"sudo\" password here.",
    "Error",
    "No consoles synced",
    "Connecting to \"%s\"...",
    "Are you sure you want to delete \"%s\"?",
    "Successfully deleted \"%s\"",
    "Rename",
    "Delete",
    "Back",
    "More",
    "Gamepad",
    "Audio",
    "Connection",
    "Webcam",
};

const char *lang(vpi_lang_t id)
{
    return lang_str[id];
}