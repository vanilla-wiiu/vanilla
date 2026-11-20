#ifndef AX88772_PROTOCOL_H
#define AX88772_PROTOCOL_H

#include <stdint.h>

#include "asix_registers.h"
#include "ethernet_definitions.h"

namespace ax88772 {

// ASIX puts a four byte header before each Ethernet frame, two bytes with
// the Ethernet frame length, and two bytes with the same length bit-inverted
// for integrity verification
constexpr uint16_t frame_header_size = 4;

struct PhyRegisters {
  uint16_t control;
  uint16_t advertisement;
  uint16_t vendor14;
  uint16_t vendor15;
  uint16_t vendor16;
};

struct Registers {
  uint8_t mac_address[6];
  uint16_t receive_control;
  uint16_t medium_mode;
  uint8_t ipg[3];
  uint8_t monitor_mode;
  uint8_t gpio_state;
  bool software_mii_enabled;
  uint8_t phy_selection;
  uint8_t software_reset;
  PhyRegisters phy;
  uint8_t multicast_filter[AX_MCAST_FILTER_SIZE];
};

struct LinkStatus {
  uint8_t bytes[8];
};

enum class FrameAssemblyResult {
  NEED_MORE,
  FRAME_READY,
  PADDING,
  INVALID
};

// Struct to keep track of Ethernet data as it comes in through chunked
// USB data (use with assemble_frame())
struct FrameAssembler {
  uint8_t header[frame_header_size];
  uint16_t header_used;
  uint16_t frame_length;
  uint16_t frame_used;
  uint8_t frame[ETH_FRAME_LEN];
};

PhyRegisters make_phy_registers();

bool phy_link_up(const Registers &registers, bool backend_link_up);

uint16_t read_phy_register(const Registers &registers, uint16_t reg, bool backend_link_up);

void write_phy_register(Registers &registers, uint16_t reg, uint16_t value);

LinkStatus make_link_status(bool link_up);

bool read_frame_header(const uint8_t *data, uint16_t available, uint16_t &frame_length);

void reset_frame_assembler(FrameAssembler &assembler);

FrameAssemblyResult assemble_frame(FrameAssembler &assembler, const uint8_t *data, uint16_t available, uint16_t &consumed);

uint16_t encode_frame(uint8_t *output, uint16_t capacity, const uint8_t *frame, uint16_t frame_length);

uint16_t prepare_frame_record(uint8_t *output, uint16_t capacity, uint16_t frame_length);

uint8_t destination_hash(const uint8_t *address);

bool accepts_destination(const Registers &registers, const uint8_t *frame, uint16_t length);

}

#endif // AX88772_PROTOCOL_H