#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#include "vanilla.h"
#include "wpa.h"

int main(int argc, const char **argv)
{
    if (argc < 2) {
        pprint("vanilla-pipe - brokers a connection between Vanilla and the Wii U\n");
        pprint("--------------------------------------------------------------------------------\n");
        pprint("\n");
        pprint("Usage: %s <wireless-interface>\n", argv[0]);
        pprint("\n");
        pprint("Connecting to the Wii U as a gamepad requires some modifications to the 802.11n\n");
        pprint("protocol, and not all platforms allow such modifications to be made.\n");
        pprint("Additionally, such modifications usually require root level access, and it can\n");
        pprint("be undesirable for various reasons to run a GUI application as root.\n");
        pprint("\n");
        pprint("This necessitated the creation of `vanilla-pipe`, a thin program that can\n");
        pprint("handle connecting to the Wii U in an environment that supports it (e.g. as root,\n");
        pprint("and in a Linux VM, or a separate Linux PC) while forwarding all data to the\n");
        pprint("user's desired frontend. `libvanilla` has full support for integrating with\n");
        pprint("`vanilla-pipe`, so as long as the frontend uses `libvanilla`, it can be used.\n");
        pprint("Additionally, since `vanilla-pipe` is fairly simple, it can be ported to\n");
        pprint("embedded devices such as MCUs or SBCs, providing more versatility in hardware\n");
        pprint("configurations.\n");
        pprint("\n");
        pprint("`vanilla-pipe` cannot be controlled directly, it can only be controlled via UDP\n");
        pprint("sockets by a compatible frontend.\n");
        pprint("\n");

        return 1;
    }

    const char *wireless_interface = argv[1];

    vanilla_listen(wireless_interface);

    return 0;
}