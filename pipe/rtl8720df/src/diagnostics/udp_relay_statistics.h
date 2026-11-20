#ifndef UDP_RELAY_STATISTICS_H
#define UDP_RELAY_STATISTICS_H

#include <stdint.h>

namespace udp_relay {

struct ChannelStatistics {
    uint32_t port;
    uint32_t console_packets;
    uint32_t console_send_errors;
    int32_t last_console_error;
    uint32_t client_packets;
    uint32_t client_send_errors;
    int32_t last_client_error;
};

struct Statistics {
    ChannelStatistics channels[5];
};

// Read-only cumulative snapshot, can be called from any task
Statistics get_statistics();

}

#endif // UDP_RELAY_STATISTICS_H
