#include "ax88772_protocol.h"

#include <string.h>

#include "asix_registers.h"
#include "mii_registers.h"

// References:
//   AX88772 datasheet: https://www.micros.com.pl/mediaserver/info-uiax88772alf.pdf
//   Linux ASIX driver code

namespace ax88772 {

// Since the Wii U requires a certain bandwidth, we only bother to
// advertise 100 Mbps full duplex. Make sure the rest of the code is
// consistent with that (and/or if you're trying to change this code
// and want more modes, you'll need to change that yourself)
//
// For reference, a real AX88772 would support four configurations:
// 10 Mbps and 100 Mbps in either full or half duplex (no gigabit)
PhyRegisters make_phy_registers()
{
  return {
    BMCR_ANENABLE | BMCR_SPEED100 | BMCR_FULLDPLX,
    ADVERTISE_100FULL | ADVERTISE_CSMA | ADVERTISE_PAUSE_CAP,
    AX88772A_PHY14H_DEFAULT,
    AX88772A_PHY15H_DEFAULT,
    AX88772A_PHY16H_DEFAULT
  };
}

uint16_t read_phy_register(const Registers &registers, uint16_t reg, bool backend_link_up)
{
  const bool link_up = phy_link_up(registers, backend_link_up);

  switch (reg) {
  case MII_BMCR:
    return registers.phy.control;
  case MII_BMSR:
  {
    uint16_t status = BMSR_100FULL | BMSR_ANEGCAPABLE | BMSR_ERCAP;
    if (link_up) {
      status |= BMSR_LSTATUS;
      if (registers.phy.control & BMCR_ANENABLE) {
        status |= BMSR_ANEGCOMPLETE;
      }
    }
    return status;
  }
  case MII_PHYSID1:
    return PHY_ID_ASIX_AX88772A >> 16;
  case MII_PHYSID2:
    return PHY_ID_ASIX_AX88772A & 0xFFFFU;
  case MII_ADVERTISE:
    return registers.phy.advertisement;
  case MII_LPA:
    if (link_up && (registers.phy.control & BMCR_ANENABLE)) {
      return LPA_100FULL | LPA_PAUSE_CAP | LPA_LPACK | ADVERTISE_CSMA;
    }
    return 0;
  case MII_EXPANSION:
    return link_up && (registers.phy.control & BMCR_ANENABLE) ? EXPANSION_NWAY : 0;
  case AX88772A_PHY14H:
    return registers.phy.vendor14;
  case AX88772A_PHY15H:
    return registers.phy.vendor15;
  case AX88772A_PHY16H:
    return registers.phy.vendor16;
  default:
    return 0;
  }
}

void write_phy_register(Registers &registers, uint16_t reg, uint16_t value)
{
  // Don't write if PHY is held in reset or powered down
  if (!(registers.software_reset & AX_SWRESET_IPRL) || (registers.software_reset & AX_SWRESET_IPPD)) {
    return;
  }

  switch (reg) {
  case MII_BMCR:
    if (value & BMCR_RESET) {
      registers.phy = make_phy_registers();
      return;
    }

    registers.phy.control = value & (
      BMCR_LOOPBACK |
      BMCR_SPEED100 |
      BMCR_ANENABLE |
      BMCR_PDOWN |
      BMCR_ISOLATE |
      BMCR_FULLDPLX |
      BMCR_CTST
    );

    if (registers.phy.control & BMCR_ANENABLE) {
      registers.phy.control |= BMCR_SPEED100 | BMCR_FULLDPLX;
    }
    break;
  case MII_ADVERTISE:
    registers.phy.advertisement = value & (
      ADVERTISE_SLCT |
      ADVERTISE_100FULL |
      ADVERTISE_PAUSE_CAP |
      ADVERTISE_PAUSE_ASYM
    );
    break;
  case AX88772A_PHY14H:
    registers.phy.vendor14 = value;
    break;
  case AX88772A_PHY15H:
    registers.phy.vendor15 = value;
    break;
  case AX88772A_PHY16H:
    registers.phy.vendor16 = value;
    break;
  default:
    // Nothing to be done
    break;
  }
}

bool phy_link_up(const Registers &registers, bool backend_link_up)
{
  if (!backend_link_up || !(registers.software_reset & AX_SWRESET_IPRL) || (registers.software_reset & AX_SWRESET_IPPD)) {
    return false;
  }

  const uint16_t control = registers.phy.control;

  if (control & (BMCR_PDOWN | BMCR_ISOLATE | BMCR_LOOPBACK | BMCR_CTST)) {
    return false;
  }

  if (control & BMCR_ANENABLE) {
    const uint16_t advertisement = registers.phy.advertisement;

    return (advertisement & ADVERTISE_SLCT) == ADVERTISE_CSMA && (advertisement & ADVERTISE_100FULL);
  } else {
    return (control & BMCR_SPEED100) && (control & BMCR_FULLDPLX);
  }
}

LinkStatus make_link_status(bool link_up)
{
  // ASIX datasheet defines eight byte interrupt packet beginning with A1 00 where byte 2, bit 0
  // reports PHY link state. e.g.
  //
  //   Link down: A1 00 00 00 00 00 00 00
  //   Link up:   A1 00 01 00 00 00 00 00
  //
  // Reference: https://www.micros.com.pl/mediaserver/info-uiax88772alf.pdf

  LinkStatus status = {};

  status.bytes[0] = 0xA1;

  if (link_up) {
    status.bytes[2] |= BIT(0);
  }

  return status;
}

bool read_frame_header(const uint8_t *data, uint16_t available, uint16_t &frame_length)
{
  frame_length = 0;

  if (data == nullptr || available < frame_header_size) {
    return false;
  }

  // ASIX sends the Ethernet frame length as a little-endian uint16, followed
  // by the same number with every bit flipped for integrity checking.
  // Read them into the host byte endianness, and read them through a byte
  // pointer rather than a uint16_t pointer because C++ requires uint16_t
  // pointers to be divisible by 2 on this platform and we might be on a
  // non-even memory address  (if the previous Ethernet frame in this buffer was
  // odd length)
  const uint16_t length = (data[0] & 0xFF) | (data[1] & 0xFF) << 8;
  const uint16_t inverted_length = (data[2] & 0xFF) | (data[3] & 0xFF) << 8;

  if ((length ^ inverted_length) != 0xFFFFU) {
    return false;
  }

  frame_length = length;
  return true;
}

void reset_frame_assembler(FrameAssembler &assembler)
{
  assembler.header_used = 0;
  assembler.frame_length = 0;
  assembler.frame_used = 0;
}

// Assemble Ethernet frame from incoming USB data chunks. The 4-byte ASIX
// header tells us the length of Ethernet frame to expect.


// Read an Ethernet frame sent by the host. USB supplies chunks of bytes;
// a chunk can end halfway through the header or frame. Save what we have
// and continue when this function is called with the next chunk.
// Read the four-byte ASIX header, then exactly the frame length it gives.
// There is no extra byte after an odd-length frame in this direction.
// Skipping one would consume the first byte of the next ASIX header.
FrameAssemblyResult assemble_frame(FrameAssembler &assembler, const uint8_t *data, uint16_t available, uint16_t &consumed)
{
  consumed = 0;

  if (data == nullptr && available != 0) {
    return FrameAssemblyResult::INVALID;
  }

  if (assembler.frame_length == 0) {
    // We don't have any data yet, but we may have read some of the header
    uint16_t count = frame_header_size - assembler.header_used;

    // Only read what is available (up to frame_header_size)
    if (count > available) {
      count = available;
    }

    // Read header remainder
    if (count != 0) {
      memcpy(assembler.header + assembler.header_used, data, count);

      assembler.header_used += count;
      consumed += count;
      data += count;
      available -= count;
    }

    // Still waiting for remainder of header
    if (assembler.header_used < frame_header_size) {
      return FrameAssemblyResult::NEED_MORE;
    }


    uint16_t length = 0;

    // Parse frame header
    if (!read_frame_header(assembler.header, frame_header_size, length)) {
      return FrameAssemblyResult::INVALID;
    }

    if (length == 0) {
      return FrameAssemblyResult::PADDING;
    }

    if (length < ETH_HLEN || length > sizeof(assembler.frame)) {
      return FrameAssemblyResult::INVALID;
    }

    // ASIX header gives us the length of the frame so we know what to expect
    assembler.frame_length = length;
  }

  // Read data payload (up to available bytes/frame length)
  uint16_t count = assembler.frame_length - assembler.frame_used;

  if (count > available) {
    count = available;
  }

  if (count != 0) {
    memcpy(assembler.frame + assembler.frame_used, data, count);

    assembler.frame_used += count;
    consumed += count;
  }

  return assembler.frame_used == assembler.frame_length ? FrameAssemblyResult::FRAME_READY : FrameAssemblyResult::NEED_MORE;
}

// Prepare Ethernet frame to send to the host. Write its 4-byte ASIX header, and if
// the frame length is odd, add a padding byte (which for some reason is only required
// from board-to-host, not host-to-board). Does not actually populate data payload,
// to do that, use encode_frame instead (or insert the data yourself). Returns total
// space used including header and padding.
uint16_t prepare_frame_record(uint8_t *output, uint16_t capacity, uint16_t frame_length)
{
  if (output == nullptr || frame_length < ETH_HLEN || frame_length > ETH_FRAME_LEN) {
    return 0;
  }

  // Get total length, rounding up for padding byte
  const uint16_t padding = frame_length & 1U;
  const uint16_t encoded_length = frame_header_size + frame_length + padding;

  if (capacity < encoded_length) {
    return 0;
  }

  // Write header
  const uint16_t inverted_length = frame_length ^ 0xFFFFU;

  output[0] = frame_length & 0xFFU;
  output[1] = frame_length >> 8;
  output[2] = inverted_length & 0xFFU;
  output[3] = inverted_length >> 8;

  // Write 0 byte padding if applicable
  if (padding != 0) {
    output[encoded_length - 1] = 0;
  }

  return encoded_length;
}

// Encapsulate an Ethernet frame into an ASIX frame for transmission over USB
uint16_t encode_frame(uint8_t *output, uint16_t capacity, const uint8_t *frame, uint16_t frame_length)
{
  if (frame == nullptr) {
    return 0;
  }

  const uint16_t encoded_length = prepare_frame_record(output, capacity, frame_length);
  if (encoded_length != 0) {
    memcpy(output + frame_header_size, frame, frame_length);
  }
  return encoded_length;
}

// Adapted from Linux v6.12 lib/crc32.c (GPL-2.0)
uint8_t destination_hash(const uint8_t *address)
{
  static constexpr uint32_t crc32_polynomial_le = 0xEDB88320U;
  uint32_t crc = 0xFFFFFFFFU;

  for (unsigned byte = 0; byte < ETH_ALEN; byte++) {
    crc ^= address[byte];

    for (unsigned bit = 0; bit < 8; bit++) {
      crc = (crc >> 1) ^ ((crc & 1U) ? crc32_polynomial_le : 0U);
    }
  }

  uint8_t index = 0;

  for (unsigned bit = 0; bit < 6; bit++) {
    index = (index << 1) | (crc & 1U);
    crc >>= 1;
  }

  return index;
}

// Determine if "we" (the Ethernet adapter) should accept an incoming Ethernet frame
// from the "outside" (which is still us in this application, but still)
bool accepts_destination(const Registers &registers, const uint8_t *frame, uint16_t length)
{
  if (!frame || length < ETH_HLEN || length > ETH_FRAME_LEN) {
    return false;
  }

  const uint16_t control = registers.receive_control;

  // If we are in "promiscuous mode", we will accept anything
  if (control & AX_RX_CTL_PRO) {
    return true;
  }

  // Determine if this is a broadcast or unicast frame (the first six bytes
  // of an Ethernet frame are the destination MAC address, and the lowest
  // bit of the first MAC address byte distinguishes whether it's for one
  // recipient or a group/broadcast). Most frames will be unicast, so check that
  // first.
  if (!(frame[0] & 1U)) {
    // This is a unicast frame. Check if it's for us.
    if (memcmp(frame, registers.mac_address, ETH_ALEN) == 0) {
      return true;
    }

    // Didn't match our MAC addres, so return false, unless we are in "AP"
    // mode, which is short for "Accept Physical Address from Multicast Filter Array"
    if (!(control & AX_RX_CTL_AP)) {
      return false;
    }
  } else {
    // This is a frame with a broadcoast/group destination address. See if
    // we should accept it.

    // Broadcast MAC address is (oops) all 0xFF
    static constexpr uint8_t broadcast[ETH_ALEN] = {
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };

    // If the frame is indeed broadcast, return true
    // (unless our "accept broadcast" flag is false)
    if (memcmp(frame, broadcast, ETH_ALEN) == 0) {
      return (control & AX_RX_CTL_AB) != 0;
    }

    // Return true if AMALL is enabled (which is short for "accept all multicast")
    if (control & AX_RX_CTL_AMALL) {
      return true;
    }

    // If AM (accept multicast) is false, return false
    if (!(control & AX_RX_CTL_AM)) {
      return false;
    }
  }

  // Default behavior - accept unicast or multicast frame if it's in our multicast filter array
  const uint8_t index = destination_hash(frame);

  return (registers.multicast_filter[index >> 3] & BIT(index & 7)) != 0;
}

}
