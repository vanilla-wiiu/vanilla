#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#include "vanilla.h"
#include "wpa.h"

void lpprint(const char *fmt, va_list args)
{
    vfprintf(stderr, fmt, args);
}

void pprint(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    lpprint(fmt, args);
    va_end(args);
}

int main(int argc, const char **argv)
{
    if (argc < 3) {
        goto show_help;
    }

    const char *wireless_interface = argv[1];
    const char *mode = argv[2];

    vanilla_install_logger(lpprint);

    if (!strcmp("-sync", mode)) {
        if (argc < 4) {
            pprint("ERROR: -sync requires sync code\n\n");
            goto show_help;
        }

        int code = atoi(argv[3]);
        if (code == 0) {
            pprint("ERROR: Invalid sync code\n\n");
            goto show_help;
        }

        vanilla_sync_with_console(wireless_interface, code);
    } else if (!strcmp("-connect", mode)) {
        vanilla_connect_to_console(wireless_interface);
    } else if (!strcmp("-is_synced", mode)) {
        if (vanilla_has_config()) {
            pprint("YES\n");
        } else {
            pprint("NO\n");
        }
    } else {
        pprint("ERROR: Invalid mode\n\n");
        goto show_help;
    }

    return 0;

show_help:
    pprint("vanilla-pipe - brokers a connection between Vanilla and the Wii U\n");
    pprint("\n");
    pprint("Usage: %s <wireless-interface> <mode> [args]\n", argv[0]);
    pprint("\n");
    pprint("Modes: \n");
    pprint("  -sync <code>                 Sync/authenticate with the Wii U.\n");
    pprint("  -connect <client-address>    Connect to the Wii U (requires syncing prior).\n");
    pprint("  -is_synced                   Returns 1 if gamepad has been synced or 0 if it hasn't yet.\n");
    pprint("\n");
    pprint("Sync code is a 4-digit PIN based on the card suits shown on the console.\n\n");
    pprint("  To calculate the code, use the following:\n");
    pprint("\n");
    pprint("    ♠ (spade) = 0\n");
    pprint("    ♥ (heart) = 1\n");
    pprint("    ♦ (diamond) = 2\n");
    pprint("    ♣ (clover) = 3\n");
    pprint("\n");
    pprint("  Example: ♣♠♥♦ (clover, spade, heart, diamond) would equal 3012\n");
    pprint("\n");

    return 1;
}