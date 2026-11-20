#ifndef USB_COMMON_DEF_H
#define USB_COMMON_DEF_H

extern "C" {
#include "usbd.h"
}

#include "ax88772_protocol.h"

// We specifically declare ourselves to be an Apple USB Ethernet Adapter,
// which internally is an AX88772, however iOS was not tolerant of our endpoint
// configuration (which is restricted by the RTL8720DF hardware) when declaring
// ourselves only as an AX88772. Luckily, this does not impact other platforms,
// which correctly still identify us as an AX88772 and can use their built-in
// drivers.
static constexpr uint8_t interrupt_in_ep        = 0x81;
static constexpr uint8_t bulk_in_ep             = 0x83;
static constexpr uint8_t bulk_out_ep            = 0x02;

static constexpr uint16_t interrupt_mps = sizeof(ax88772::LinkStatus);
static constexpr uint16_t high_speed_bulk_mps   = 512;
static constexpr uint16_t language_id           = 0x0409; // English (US)

// String index 0 - supported language IDs
static uint8_t language_descriptor[] USB_DMA_ALIGNED = {
    4,
    USB_DESC_TYPE_STRING,
    USB_LOW_BYTE(language_id),
    USB_HIGH_BYTE(language_id)
};
static_assert(sizeof(language_descriptor) == 4, "Incorrect language descriptor size");

// String index 3 - temporary serial number
static uint8_t serial_descriptor[] USB_DMA_ALIGNED = {
    26,
    USB_DESC_TYPE_STRING,
    '0', 0, '0', 0, '0', 0, '0', 0,
    '0', 0, '0', 0, '0', 0, '0', 0,
    '0', 0, '0', 0, '0', 0, '1', 0
};
static_assert(sizeof(serial_descriptor) == 26, "Incorrect serial descriptor size");

#endif // USB_COMMON_DEF_H