#include "lang.h"

#include "vanilla.h"

static const char *lang_str[] = {
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
    "Controls",
    "Audio",
    "Connection",
    "Webcam",
    "Region",
    "Local",
    "Via Server",
    "Set up how Vanilla connects to the Wii U",
    "Choose 'Local' to connect directly with the hardware on your system. Otherwise, you may need a compatible 'server' to relay the necessary packets.",
    "Select the wireless interface to use for the connection.",
    "Enter the IP address of the server you wish to connect through.",
    "Screenshot saved to \"%s\"",
    "Recording started to \"%s\"",
    "Recording finished",
    "Connection lost, attempting to re-connect...",
    "Your platform is not capable of \"Local\" connection, and must connect via a server.",
    "Failed to save screenshot (%i)",
    "Select gamepad region",
    "Gamepad region must match the console region or else the console will throw an error. This has no other effect on usage.",
    "Japan",
    "America",
    "Europe",
    "Quit",
	"Authentication failed. Please try again.",
	"Don't ask again",
	"WARNING",
	"This option will install a Polkit rule that will allow Vanilla's backend to run as root without password entry.\n\nThere are inherent risks to bypassing security systems. Only enable this if you are willing to accept those risks.\n\nNOTE: If you are using the Steam Deck, this will also disable read-only access to your system partition.",
	"I acknowledge",
	"Root Password Skip",
	"Root Password Skip",
	"Swap AB/XY buttons",
	"Keyboard Controls",
    "Reset To Defaults",
    "Reset",
    "Full Screen",
    "Hardware Decoding",
    "Cursor In Full Screen",
    "Local connection on Android requires root access.",
    "Vanilla Menu",
    "Record Video",
    "Stop Recording",
    "Take Screenshot",
    "Blow Microphone",
    "NFC",
    "Disconnect",
    "Close",
    "Success",
    "Unknown Error",
    "Backend reported that we sent an unknown command. This is not expected to happen, file a bug report.",
    "Backend reported that we sent an invalid argument. This is not expected to happen, file a bug report.",
    "Failed to communicate with pipe. If this is a \"via server\" connection, that probably means it is inaccessible on this network. If this is a \"local\" connection, you may need to restart Vanilla or restart your computer.",
    "Out of memory",
    "Pipe reported that it was busy, try again later (or check the status of the pipe)",
    "Unable to create sockets to communicate with Wii U. This probably means another program on your computer is using the ports Vanilla needs. If it's a previous instance of Vanilla that you are unable to close, try force quitting it or restarting your computer.",
    "Failed to read or write necessary configuration to the disk. This may be a permissions issue or you may be running out of disk space.",
    "Failed to initialize wpa_supplicant",
    "Failed to perform DHCP handshake",
    "Failed to initialize video decoder",
    "Failed to fork process",
    "%s (%i)\n\nCheck logs for more information",
    "Failed to launch pipe with required permissions",
    "Your Wi-Fi adapter found a Wii U but refused to connect to it. This most likely means your Wi-Fi adapter is incompatible or needs a modified firmware to work.",
    "The Wii U rejected Vanilla. You may need to re-sync with it.",
    "You are currently running the Windows version of Vanilla in Wine. This is most likely a mistake; the Windows version of Vanilla is far more limited than the native Linux version.\n\n"
        "It is highly recommended you switch to the native Linux version instead. If you understand what you are doing and wish to continue, click OK.",
    "Your Wi-Fi adapter is not capable of 5GHz, which is required for connecting to the Wii U. Please try another one.",
    "Your Wi-Fi adapter's 5GHz frequencies, which are required for connecting to the Wii U, are disabled. This may be due to your system's Wi-Fi regulatory region settings. Please try changing these settings or try another Wi-Fi adapter.",
};

static_assert(sizeof(lang_str)/sizeof(const char *) == __VPI_LANG_T_COUNT, "Missing language entries");

const char *lang(vpi_lang_t id)
{
    return lang_str[id];
}

vpi_lang_t lang_error(int error)
{
    switch (error) {
    case VANILLA_SUCCESS:
        return VPI_LANG_SUCCESS;
    case VANILLA_ERR_UNKNOWN_COMMAND:
        return VPI_LANG_UNKNOWN_COMMAND;
    case VANILLA_ERR_INVALID_ARGUMENT:
        return VPI_LANG_INVALID_ARGUMENT;
    case VANILLA_ERR_PIPE_UNRESPONSIVE:
        return VPI_LANG_PIPE_UNRESPONSIVE;
    case VANILLA_ERR_OUT_OF_MEMORY:
        return VPI_LANG_OUT_OF_MEMORY;
    case VANILLA_ERR_BUSY:
        return VPI_LANG_BUSY;
    case VANILLA_ERR_BAD_SOCKET:
        return VPI_LANG_BAD_SOCKET;
    case VANILLA_ERR_FILESYSTEM:
        return VPI_LANG_FILESYSTEM;
    case VANILLA_ERR_WPA_SUPPLICANT_INIT:
        return VPI_LANG_WPA_SUPPLICANT;
    case VANILLA_ERR_DHCP:
        return VPI_LANG_DHCP;
    case VANILLA_ERR_VIDEO_DECODER:
        return VPI_LANG_VIDEO_DECODER;
    case VANILLA_ERR_FORK:
        return VPI_LANG_FORK;
    case VANILLA_ERR_PKEXEC:
        return VPI_LANG_PKEXEC;
    case VANILLA_ERR_ASSOC_REJECT:
        return VPI_LANG_ASSOC_REJECT;
    case VANILLA_ERR_AUTH_REJECT:
        return VPI_LANG_AUTH_REJECT;
    case VANILLA_ERR_NO_5GHZ:
        return VPI_LANG_NO_5GHZ;
    case VANILLA_ERR_NO_ENABLED_5GHZ:
        return VPI_LANG_NO_ENABLED_5GHZ;

    // These errors are not expected to show so we don't have a string for them
    case VANILLA_ERR_SYNC:
    case VANILLA_ERR_SHUTDOWN:
    case VANILLA_ERR_CONNECTED:
    case VANILLA_ERR_DISCONNECTED:
    case VANILLA_ERR_INTERRUPTED:
    case VANILLA_ERR_CLEANUP:
    default:
        return VPI_LANG_UNKNOWN_ERROR;
    }
}

const char *lang_error_str(int error)
{
    return lang(lang_error(error));
}
