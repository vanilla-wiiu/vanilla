#include "wifi_diagnostics.h"

#if VANILLA_DIAGNOSTICS
extern "C" {
#include "FreeRTOS.h"
#include "task.h"
}
#include <wifi_ind.h>

namespace wifi_diagnostics {

static Statistics statistics = {0, 0, 0, 0, -1};
static bool started = false;

// Don't query the driver or print in these SDK callbacks, just record bounded metadata
static void connected(char *, int, int, void *)
{
    const uint32_t now = xTaskGetTickCount();
    taskENTER_CRITICAL();
    ++statistics.connect_events;
    statistics.last_connect_tick = now;
    taskEXIT_CRITICAL();
}

static void disconnected(char *buffer, int length, int, void *)
{
    int32_t reason = -1;
    if (buffer != nullptr && length >= 8) {
        // SDK payload provides six MAC bytes then a little-endian disconnect reason
        const auto *bytes = reinterpret_cast<const uint8_t *>(buffer);
        reason = bytes[6] | (static_cast<uint32_t>(bytes[7]) << 8);
    }
    const uint32_t now = xTaskGetTickCount();
    taskENTER_CRITICAL();
    ++statistics.disconnect_events;
    statistics.last_disconnect_tick = now;
    statistics.last_disconnect_reason = reason;
    taskEXIT_CRITICAL();
}

void begin()
{
    if (started) {
        return;
    }
    taskENTER_CRITICAL();
    wifi_reg_event_handler(WIFI_EVENT_CONNECT, connected, nullptr);
    wifi_reg_event_handler(WIFI_EVENT_DISCONNECT, disconnected, nullptr);
    taskEXIT_CRITICAL();
    started = true;
}

Statistics get_statistics()
{
    taskENTER_CRITICAL();
    const Statistics snapshot = statistics;
    taskEXIT_CRITICAL();
    return snapshot;
}

}
#endif // VANILLA_DIAGNOSTICS
