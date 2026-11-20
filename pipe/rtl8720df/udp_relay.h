#ifndef UDP_RELAY_H
#define UDP_RELAY_H

#include "src/diagnostics/udp_relay_statistics.h"

extern "C" {
#include "lwip/ip_addr.h"
}

struct netif;

namespace udp_relay {

// Call these functions from lwIP thread
bool begin(struct netif *usb, struct netif *wifi);
void set_session(const ip_addr_t &client, bool enabled);

}

#endif // UDP_RELAY_H