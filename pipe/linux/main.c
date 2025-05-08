#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "vanilla.h"
#include "wpa.h"

int main(int argc, const char **argv)
{
    if (geteuid() != 0) {
        nlprint("vanilla-pipe must be run as root");
        return 1;
    }

    if (argc != 3) {
        nlprint("vanilla-pipe - brokers a connection between Vanilla and the Wii U");
        nlprint("--------------------------------------------------------------------------------");
        nlprint("");
        nlprint("Usage: %s <-local | -udp> <wireless-interface>", argv[0]);
        nlprint("");
        nlprint("Connecting to the Wii U as a gamepad requires some modifications to the 802.11n");
        nlprint("protocol, and not all platforms allow such modifications to be made.");
        nlprint("Additionally, such modifications usually require root level access, and it can");
        nlprint("be undesirable for various reasons to run a GUI application as root.");
        nlprint("");
        nlprint("This necessitated the creation of `vanilla-pipe`, a thin program that can");
        nlprint("handle connecting to the Wii U in an environment that supports it (e.g. as root,");
        nlprint("and in a Linux VM, or a separate Linux PC) while forwarding all data to the");
        nlprint("user's desired frontend. `libvanilla` has full support for integrating with");
        nlprint("`vanilla-pipe`, so as long as the frontend uses `libvanilla`, it can be used.");
        nlprint("Additionally, since `vanilla-pipe` is fairly simple, it can be ported to");
        nlprint("embedded devices such as MCUs or SBCs, providing more versatility in hardware");
        nlprint("configurations.");
        nlprint("");
        nlprint("`vanilla-pipe` cannot be controlled directly, it can only be controlled via");
        nlprint("sockets by a compatible frontend. By choosing '-local' or '-udp', you can");
        nlprint("choose what type of socket to use to best suit the environment.");
        nlprint("");

        return 1;
    }

    int udp_mode = 0;
    int local_mode = 0;
    const char *wireless_interface = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-udp")) {
            udp_mode = 1;
        } else if (!strcmp(argv[i], "-local")) {
            local_mode = 1;
        } else {
            wireless_interface = argv[i];
        }
    }

    if (udp_mode == local_mode) {
        nlprint("Must choose either '-local' OR '-udp'");
        return 1;
    }

    if (!wireless_interface) {
        nlprint("Must identify a wireless interface to use");
        return 1;
    }

    pipe_listen(local_mode, wireless_interface);

    return 0;
}