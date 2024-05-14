#include "gamepad.h"

#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
//#include <sys/socket.h>

#include "status.h"
#include "util.h"

static const uint16_t PORT_MSG = 50110;
static const uint16_t PORT_VID = 50120;
static const uint16_t PORT_AUD = 50121;
static const uint16_t PORT_HID = 50122;
static const uint16_t PORT_CMD = 50123;

static int socket_video;

char wireless_connect_config_filename[1024] = {0};
const char *get_wireless_connect_config_filename()
{
    if (wireless_connect_config_filename[0] == 0) {
        // Not initialized yet, do this now
        get_home_directory_file("vanilla_wpa_connect.conf", wireless_connect_config_filename, sizeof(wireless_connect_config_filename));
    }
    return wireless_connect_config_filename;
}

int connect_as_gamepad_internal(const char *wireless_interface)
{
    /*
    // connect to Wii U with wpa_supplicant
        // kill any existing wpa_supplicant_drc
        // rfkill unblock wlan?
    // dhclient on the network interface
    // set region

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;

    // Set up video receiving feed
    address.sin_port = htons(PORT_VID);
    socket_video = socket(AF_INET, SOCK_DGRAM, 0);
    // TODO: Limit to one interface...
    if (bind(socket_video, (const struct sockaddr *) &address, sizeof(address)) == -1) {
        return VANILLA_ERROR;
    }

    while (1) {
        unsigned char data[2048];
        memset(data, 0, sizeof(data));

        ssize_t size = recv(socket_video, &data, sizeof(data), 0);
        printf("received %li\n", size);
    }
    */
   return 0;
}