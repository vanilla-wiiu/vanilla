#ifndef VANILLA_PIPE_H
#define VANILLA_PIPE_H

struct netif;

namespace vanilla_pipe {

// Call from lwIP thread
bool begin(struct netif *interface);

// Call from Arduino loop(), outside lwIP thread
void service();

// Call from lwIP when USB Ethernet link is lost
void link_down();

}

#endif // VANILLA_PIPE_H