#ifndef VANILLA_PI_LANG_H
#define VANILLA_PI_LANG_H

typedef enum {
    VPI_LANG_SYNC_BTN,
    VPI_LANG_CONNECT_BTN,
    VPI_LANG_EDIT_BTN,
    VPI_LANG_SETTINGS_BTN,
    VPI_LANG_OK_BTN,
    VPI_LANG_CANCEL_BTN,
    VPI_LANG_SYNC_HELP_1,
    VPI_LANG_SYNC_HELP_2,
    VPI_LANG_SYNC_CONNECTING,
    VPI_LANG_SYNC_FAILED,
    VPI_LANG_SUDO_HELP_1,
    VPI_LANG_SUDO_HELP_2,
    VPI_LANG_ERROR,
    VPI_LANG_NO_CONSOLES_SYNCED,
    VPI_LANG_CONNECTING_TO,
    VPI_LANG_DELETE_CONFIRM,
    VPI_LANG_DELETE_SUCCESS,
    VPI_LANG_RENAME,
    VPI_LANG_DELETE,
    VPI_LANG_BACK,
    VPI_LANG_MORE,
    VPI_LANG_GAMEPAD,
    VPI_LANG_AUDIO,
    VPI_LANG_CONNECTION,
    VPI_LANG_WEBCAM,
    VPI_LANG_REGION,
    VPI_LANG_LOCAL,
    VPI_LANG_VIA_SERVER,
    VPI_LANG_CONNECTION_HELP_1,
    VPI_LANG_CONNECTION_HELP_2,
    VPI_LANG_LOCAL_CONNECTION_HELP,
    VPI_LANG_SERVER_CONNECTION_HELP,
    VPI_LANG_SCREENSHOT_SUCCESS,
    VPI_LANG_RECORDING_START,
    VPI_LANG_RECORDING_FINISH,
    VPI_LANG_DISCONNECTED,
    VPI_LANG_NO_LOCAL_AVAILABLE,
    VPI_LANG_SCREENSHOT_ERROR,
    VPI_LANG_REGION_HELP_1,
    VPI_LANG_REGION_HELP_2,
    VPI_LANG_REGION_JAPAN,
    VPI_LANG_REGION_AMERICA,
    VPI_LANG_REGION_EUROPE,
    VPI_LANG_QUIT,
    __VPI_LANG_T_COUNT
} vpi_lang_t;

const char *lang(vpi_lang_t id);

#endif // VANILLA_PI_LANG_H