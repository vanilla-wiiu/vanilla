#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#include "wpa.h"

int main(int argc, const char **argv)
{
    if (argc < 3) {
        goto show_help;
    }

    const char *wireless_interface = argv[1];
    const char *mode = argv[2];


    if (!strcmp("-sync", mode)) {
        if (argc < 3) {
            printf("ERROR: -sync requires sync code\n\n");
            goto show_help;
        }

        int code = atoi(argv[3]);
        if (code == 0) {
            printf("ERROR: Invalid sync code\n\n");
            goto show_help;
        }

        vanilla_sync_with_console(wireless_interface, code);
    } else if (!strcmp("-connect", mode)) {
        if (argc < 3) {
            printf("ERROR: -sync requires local address\n\n");
            goto show_help;
        }

        if (!inet_pton(AF_INET, argv[3], &client_address)) {
            printf("ERROR: Invalid client address\n\n");
            goto show_help;
        }

        vanilla_connect_to_console(wireless_interface);
    } else if (!strcmp("-is_synced", mode)) {
        if (vanilla_has_config()) {
            printf("YES\n");
        } else {
            printf("NO\n");
        }
    } else {
        printf("ERROR: Invalid mode\n\n");
        goto show_help;
    }

    return 0;

show_help:
    printf("vanilla-pipe - brokers a connection between Vanilla and the Wii U\n");
    printf("\n");
    printf("Usage: %s <wireless-interface> <mode> [args]\n", argv[0]);
    printf("\n");
    printf("Modes: \n");
    printf("  -sync <code>                 Sync/authenticate with the Wii U\n");
    printf("  -connect <client-address>    Connect to the Wii U (requires syncing prior)\n");
    printf("  -is_synced                   Returns 1 if gamepad has been synced or 0 if it hasn't yet\n");
    printf("\n");
    printf("Sync code is a 4-digit PIN based on the card suits shown on the console.\n");
    printf("To calculate the code, use the following:\n");
    printf("  ♠ (spade) = 0 ♥ (heart) = 1 ♦ (diamond) = 2 ♣ (clover) = 3\n");
    printf("  Example: ♣♠♥♦ (clover, spade, heart, diamond) would equal 3012\n");
    printf("\n");

    return 1;
}