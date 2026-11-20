#ifndef ETHERNET_BACKEND_H
#define ETHERNET_BACKEND_H

extern "C" {
#include "usb_os.h"
}

#include <stdint.h>

#include "ethernet_definitions.h"
#include "ax88772_protocol.h"

namespace ethernet {

// Size of the USB buffer
static constexpr uint16_t transmit_buffer_size = 2048;

// lwIP will cut the Wii U's larger (up to 1,800 bytes) frames into two since
// Ethernet's MTU is only 1,500. But since we can transfer 2,048 USB bytes at
// a time, we can at least cut down on USB overhead by transferring the two
// split frames together
static constexpr uint8_t transmit_records_per_buffer = 2;

// Struct to keep track of outgoing USB data
struct USB_DMA_ALIGNED TransmitFrame {
  uint8_t data[transmit_buffer_size];
  uint16_t frame_lengths[transmit_records_per_buffer]; // Ethernet frame bytes only
  uint16_t length; // Total bytes to send, including ASIX headers and padding
  uint8_t frame_count;
};

struct TransmitStatistics {
  uint32_t queued;
  uint32_t pool_empty;
};

struct ReceiveStatistics {
  uint32_t submitted;
  uint32_t dropped;
};

bool begin();

// Receive Ethernet frame from host. This function will copy the data you submit
// and send it to lwIP.
//
// Caller holds USB state lock
bool receive_frame(const uint8_t *data, uint16_t length);

// Scoped class to batch Ethernet frames together into a single USB transfer
// as lwIP splits them.
//
// Create this on the lwIP thread around one UDP send. If lwIP splits that
// packet, its Ethernet frames can share a USB buffer. Queue the buffer
// when full or when the outermost TransmitBatch is destroyed; never wait
// for another UDP packet. Once queued, only USB may use it until returned.
class TransmitBatch {
public:
  TransmitBatch();
  ~TransmitBatch();
  TransmitBatch(const TransmitBatch &) = delete;
  TransmitBatch &operator=(const TransmitBatch &) = delete;
};

/// Give USB ownership of the oldest ready TX buffer, without waiting.
/// Return false if TX is disabled or no buffer is ready. On success, return the
/// buffer exactly once with release_transmit_frame() after USB finishes using it.
bool take_transmit_frame(TransmitFrame *&frame);

/// Return a buffer to the free ring only after its current owner stops using it
void release_transmit_frame(TransmitFrame *frame);

void set_transmit_enabled(bool enabled);

/// Serialized with receive_frame() by the USB state lock
void set_receive_enabled(bool enabled);

/// Requests link change on lwIP TCP/IP thread
void request_link_state(bool up);

/// Retry scheduling outstanding changes
void service_link_state();

// Whether the backend netif/services finished initialization. This remains true
// across USB reconnects; it does not indicate that the current link is up.
bool is_ready();

TransmitStatistics get_transmit_statistics();
ReceiveStatistics get_receive_statistics();

}

#endif // ETHERNET_BACKEND_H
