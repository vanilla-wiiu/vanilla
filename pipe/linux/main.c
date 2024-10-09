#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#include "vanilla.h"
#include "wpa.h"

int main(int argc, const char **argv)
{
    if (argc < 2) {
        pprint("vanilla-pipe - brokers a connection between Vanilla and the Wii U\n");
        pprint("\n");
        pprint("Usage: %s <wireless-interface> <mode> [args]\n", argv[0]);
        pprint("\n");
        pprint("Modes: \n");
        pprint("  -sync <code>  Sync/authenticate with the Wii U.\n");
        pprint("  -connect      Connect to the Wii U (requires syncing prior).\n");
        pprint("  -is_synced    Returns 1 if gamepad has been synced or 0 if it hasn't yet.\n");
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

    const char *wireless_interface = argv[1];

    vanilla_listen(wireless_interface);

    return 0;
}