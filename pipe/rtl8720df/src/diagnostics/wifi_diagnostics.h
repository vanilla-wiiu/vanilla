#ifndef WIFI_DIAGNOSTICS_H
#define WIFI_DIAGNOSTICS_H

#include <stdint.h>
#include "../../build_config.h"

#if VANILLA_DIAGNOSTICS
namespace wifi_diagnostics {

struct Statistics {
    uint32_t connect_events;
    uint32_t disconnect_events;
    uint32_t last_connect_tick;
    uint32_t last_disconnect_tick;
    int32_t last_disconnect_reason;
};

// Call only from Arduino context after driver initialization
void begin();

// Can be called from any task
Statistics get_statistics();

}
#endif // VANILLA_DIAGNOSTICS

#endif // WIFI_DIAGNOSTICS_H
