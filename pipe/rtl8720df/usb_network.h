#ifndef USB_NETWORK_H
#define USB_NETWORK_H

#include <stdint.h>
#include "src/diagnostics/usb_network_diagnostics.h"

bool usb_network_begin();

uint32_t usb_network_service_pass_count();

// Tell USB backend that we have data buffers ready to be sent out (made by
// the Ethernet/ASIX backends). Reduces delays and avoids waking another task
// unnecessarily for every packet. Call outside of backend critical section.
void usb_network_notify_transmit();

#endif // USB_NETWORK_H
