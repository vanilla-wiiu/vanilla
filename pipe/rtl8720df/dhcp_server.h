#ifndef VANILLA_PIPE_DHCP_SERVER_H
#define VANILLA_PIPE_DHCP_SERVER_H

struct netif;

namespace dhcp {

// Call these functions from lwIP thread
bool begin(struct netif *interface);
void stop();
void link_down();

}

#endif // VANILLA_PIPE_DHCP_SERVER_H