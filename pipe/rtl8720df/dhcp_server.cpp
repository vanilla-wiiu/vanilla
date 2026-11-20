#include "dhcp_server.h"

extern "C" {
#include "lwip/def.h"
#include "lwip/err.h"
#include "lwip/ip.h"
#include "lwip/ip4.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/prot/dhcp.h"
#include "lwip/udp.h"
#include "lwip/sys.h"
}

#include <string.h>

#include "src/diagnostics/debug_log.h"
#include "ethernet_definitions.h"

namespace dhcp {

static struct udp_pcb *server_pcb = nullptr;
static struct netif *server_interface = nullptr;

enum class OptionResult {
  VALUE,
  END,
  INVALID
};

struct Option {
  u8_t code;
  u8_t length;
  u16_t offset;
};

struct RequestOptions {
  u8_t message_type;
  ip4_addr_t requested_ip;
  ip4_addr_t server_id;
  bool has_requested_ip;
  bool has_server_id;
};

enum class RequestKind {
  IGNORE,
  DISCOVER,
  SELECTING,
  INIT_REBOOT,
  RENEW_OR_REBIND,
  DECLINE,
  RELEASE,
  INFORM
};

static constexpr u32_t lease_seconds = 3600;
static constexpr u32_t offer_hold_ms = 30 * 1000;
static constexpr u32_t decline_hold_ms = 60 * 1000;

enum class LeaseState {
  FREE,
  OFFERED,
  BOUND,
  DECLINED
};

static constexpr u16_t client_identity_capacity = ETH_FRAME_LEN;

struct ClientIdentity {
  bool uses_client_id;
  u16_t length;
  u8_t data[client_identity_capacity];
};

struct Lease {
  LeaseState state;
  u32_t started_at_ms;
  u32_t duration_ms;
  ClientIdentity owner;
};

static Lease lease = {};
static bool lease_disconnected = false;

static ip4_addr_t lease_address = {};

enum class ReplyKind {
  NONE,
  OFFER,
  ACK,
  NAK,
  INFORM_ACK
};

static constexpr u16_t dhcp_minimum_ip_size = 576;

struct ReplyBuffer {
  u8_t data[ETH_DATA_LEN];
  u16_t length;
  u16_t capacity;
};

static bool same_client (const ClientIdentity &a, const ClientIdentity &b)
{
  return a.uses_client_id == b.uses_client_id
      && a.length == b.length
      && memcmp(a.data, b.data, a.length) == 0;
}

static void set_lease_state(LeaseState state, u32_t now)
{
  u32_t duration = 0;

  switch (state) {
  case LeaseState::FREE:
    lease.owner = {};
    break;
  case LeaseState::OFFERED:
    duration = offer_hold_ms;
    break;
  case LeaseState::BOUND:
    duration = lease_seconds * 1000;
    break;
  case LeaseState::DECLINED:
    duration = decline_hold_ms;
    break;
  }

  lease_disconnected = false;
  lease.state = state;
  lease.started_at_ms = now;
  lease.duration_ms = duration;
}

static void expire_lease(u32_t now)
{
  if (lease.state == LeaseState::FREE) {
    return;
  }

  const u32_t elapsed = now - lease.started_at_ms;

  if (elapsed >= lease.duration_ms) {
    set_lease_state(LeaseState::FREE, now);
  }
}

static bool read_header(const struct pbuf *packet, struct dhcp_msg &header)
{
  if (packet == nullptr || packet->tot_len < DHCP_OPTIONS_OFS) {
    return false;
  }

  header = {};

  if (pbuf_copy_partial(packet, &header, DHCP_OPTIONS_OFS, 0) != DHCP_OPTIONS_OFS) {
    return false;
  }

  return header.op == DHCP_BOOTREQUEST &&
         header.htype == DHCP_HTYPE_ETH &&
         header.hlen == ETH_ALEN &&
         lwip_ntohl(header.cookie) == DHCP_MAGIC_COOKIE;
}

static OptionResult next_option(const struct pbuf *packet, u16_t &offset, u16_t end, Option &option)
{
  if (packet == nullptr || offset > end || end > packet->tot_len) {
    return OptionResult::INVALID;
  }

  while (offset < end) {
    const u8_t code = pbuf_get_at(packet, offset++);

    if (code == DHCP_OPTION_PAD) {
      continue;
    }

    if (code == DHCP_OPTION_END) {
      return OptionResult::END;
    }

    if (offset == end) {
      return OptionResult::INVALID;
    }

    const u8_t length = pbuf_get_at(packet, offset++);

    if (length > end - offset) {
      return OptionResult::INVALID;
    }

    option.code = code;
    option.length = length;
    option.offset = offset;
    offset += length;

    return OptionResult::VALUE;
  }

  return OptionResult::INVALID;
}

static bool collect_option(const struct pbuf *packet, u16_t offset, u16_t end, u8_t code, void *value, u16_t capacity, u16_t &used, bool &present)
{
  auto *output = static_cast<u8_t *>(value);
  Option option = {};
  OptionResult result;

  while ((result = next_option(packet, offset, end, option)) == OptionResult::VALUE) {
    if (option.code != code) {
      continue;
    }

    present = true;

    if (option.length > capacity - used) {
      return false;
    }

    if (pbuf_copy_partial(packet, output + used, option.length, option.offset) != option.length) {
      return false;
    }

    used += option.length;
  }

  return result == OptionResult::END;
}

static bool read_option_bytes(const struct pbuf *packet, u8_t code, void *value, u16_t capacity, u16_t &length, bool &present)
{
  length = 0;
  present = false;

  if (!packet) {
    return false;
  }

  u8_t overload = DHCP_OVERLOAD_NONE;
  u16_t overload_used = 0;
  bool has_overload = false;

  if (!collect_option(packet, DHCP_OPTIONS_OFS, packet->tot_len, DHCP_OPTION_OVERLOAD, &overload, sizeof(overload), overload_used, has_overload)) {
    return false;
  }

  if (has_overload && (overload_used != sizeof(overload) || overload == DHCP_OVERLOAD_NONE || overload > DHCP_OVERLOAD_SNAME_FILE)) {
    return false;
  }

  u16_t used = 0;
  present = false;

  if (!collect_option(packet, DHCP_OPTIONS_OFS, packet->tot_len, code, value, capacity, used, present)) {
    return false;
  }

  if ((overload & DHCP_OVERLOAD_FILE)
    && !collect_option(packet, DHCP_FILE_OFS, DHCP_FILE_OFS + DHCP_FILE_LEN, code, value, capacity, used, present)) {
    return false;
  }

  if ((overload & DHCP_OVERLOAD_SNAME)
    && !collect_option(packet, DHCP_SNAME_OFS, DHCP_SNAME_OFS + DHCP_SNAME_LEN, code, value, capacity, used, present)) {
    return false;
  }

  length = used;
  return true;
}

static bool read_option_value(const struct pbuf *packet, u8_t code, void *value, u16_t size, bool &present)
{
  u16_t length = 0;
  return read_option_bytes(packet, code, value, size, length, present) && (!present || length == size);
}

static bool read_client_identity(const struct pbuf *packet, const struct dhcp_msg &header, ClientIdentity &identity)
{
  identity = {};

  if (!read_option_bytes(packet, DHCP_OPTION_CLIENT_ID, identity.data, sizeof(identity.data), identity.length, identity.uses_client_id)) {
    return false;
  }

  if (identity.uses_client_id) {
    return identity.length >= 2;
  }

  identity.length = ETH_ALEN;
  memcpy(identity.data, header.chaddr, identity.length);

  return true;
}

static bool read_request_options(const struct pbuf *packet, RequestOptions &options)
{
  options = {};
  bool has_message_type = false;

  return read_option_value(
    packet, DHCP_OPTION_MESSAGE_TYPE, &options.message_type, sizeof(options.message_type), has_message_type
  ) && has_message_type && read_option_value(
    packet, DHCP_OPTION_REQUESTED_IP, &options.requested_ip.addr, sizeof(options.requested_ip.addr), options.has_requested_ip
  ) && read_option_value(
    packet, DHCP_OPTION_SERVER_ID, &options.server_id.addr, sizeof(options.server_id.addr), options.has_server_id
  );
}

static RequestKind classify_request(const struct netif *interface, const struct dhcp_msg &header, const RequestOptions &options)
{
  if (!ip4_addr_isany_val(header.giaddr)) {
    return RequestKind::IGNORE;
  }

  if (options.has_server_id && !ip4_addr_cmp(&options.server_id, netif_ip4_addr(interface))) {
    return RequestKind::IGNORE;
  }

  const bool has_current_ip = !ip4_addr_isany_val(header.ciaddr);

  switch (options.message_type) {
  case DHCP_DISCOVER:
    if (!options.has_server_id && !has_current_ip) {
      return RequestKind::DISCOVER;
    }
    break;
  case DHCP_REQUEST:
    if (options.has_requested_ip) {
      if (has_current_ip) {
        break;
      }

      return options.has_server_id ? RequestKind::SELECTING : RequestKind::INIT_REBOOT;
    }

    if (!options.has_server_id && has_current_ip) {
      return RequestKind::RENEW_OR_REBIND;
    }
    break;
  case DHCP_DECLINE:
    if (options.has_server_id && options.has_requested_ip && !has_current_ip) {
      return RequestKind::DECLINE;
    }
    break;
  case DHCP_RELEASE:
    if (options.has_server_id && !options.has_requested_ip && has_current_ip) {
      return RequestKind::RELEASE;
    }
    break;
  case DHCP_INFORM:
    if (!options.has_server_id && !options.has_requested_ip && has_current_ip) {
      return RequestKind::INFORM;
    }
    break;
  default:
    break;
  }

  return RequestKind::IGNORE;
}

static ReplyKind select_reply(const struct netif *interface, RequestKind kind, const struct dhcp_msg &header, const RequestOptions &options, const ClientIdentity &client, u32_t now)
{
  if (lease_disconnected) {
    lease_disconnected = false;

    if (lease.state == LeaseState::BOUND && !same_client(lease.owner, client)) {
      set_lease_state(LeaseState::FREE, now);
    }
  }

  const bool owns_lease = (lease.state == LeaseState::OFFERED || lease.state == LeaseState::BOUND) && same_client(lease.owner, client);

  ip4_addr_t current_ip;
  ip4_addr_copy(current_ip, header.ciaddr);

  switch (kind) {
  case RequestKind::DISCOVER:
    if (lease.state == LeaseState::FREE || owns_lease) {
      return ReplyKind::OFFER;
    }
    break;
  case RequestKind::SELECTING:
    if (!ip4_addr_cmp(&options.requested_ip, &lease_address)) {
      return ReplyKind::NAK;
    }

    if (lease.state == LeaseState::FREE || owns_lease) {
      return ReplyKind::ACK;
    }

    return ReplyKind::NAK;
  case RequestKind::INIT_REBOOT:
    if (!ip4_addr_netcmp(&options.requested_ip, netif_ip4_addr(interface), netif_ip4_netmask(interface))) {
      return ReplyKind::NAK;
    }

    if (lease.state == LeaseState::BOUND && owns_lease) {
      return ip4_addr_cmp(&options.requested_ip, &lease_address) ? ReplyKind::ACK : ReplyKind::NAK;
    }
    break;
  case RequestKind::RENEW_OR_REBIND:
    if (!ip4_addr_netcmp(&current_ip, netif_ip4_addr(interface), netif_ip4_netmask(interface))) {
      return ReplyKind::NAK;
    }

    if (lease.state == LeaseState::BOUND && owns_lease) {
      return ip4_addr_cmp(&current_ip, &lease_address) ? ReplyKind::ACK : ReplyKind::NAK;
    }
    break;
  case RequestKind::DECLINE:
    if (owns_lease && ip4_addr_cmp(&options.requested_ip, &lease_address)) {
      set_lease_state(LeaseState::DECLINED, now);
    }
    break;
  case RequestKind::RELEASE:
    if (lease.state == LeaseState::BOUND && owns_lease && ip4_addr_cmp(&current_ip, &lease_address)) {
      set_lease_state(LeaseState::FREE, now);
    }
    break;
  case RequestKind::INFORM:
    if (ip4_addr_netcmp(&current_ip, netif_ip4_addr(interface), netif_ip4_netmask(interface))
     && !ip4_addr_cmp(&current_ip, netif_ip4_addr(interface))
     && !ip4_addr_isbroadcast(&current_ip, interface)
     && (current_ip.addr & ~netif_ip4_netmask(interface)->addr) != 0) {
      return ReplyKind::INFORM_ACK;
     }
    break;
  case RequestKind::IGNORE:
    break;
  }

  return ReplyKind::NONE;
}

static bool append_option(ReplyBuffer &output, u8_t code, const void *value, u16_t length)
{
  if (output.capacity > sizeof(output.data) || output.length > output.capacity || (value == nullptr && length != 0)) {
    return false;
  }

  const auto *bytes = static_cast<const u8_t *>(value);

  do {
    const u8_t part = length > 255 ? 255 : length;

    if (part + 2 > output.capacity - output.length) {
      return false;
    }

    output.data[output.length++] = code;
    output.data[output.length++] = part;

    if (part != 0) {
      memcpy(output.data + output.length, bytes, part);
      bytes += part;
      output.length += part;
    }

    length -= part;
  } while (length != 0);

  return true;
}

static bool build_reply(const struct pbuf *packet, const struct netif *interface, const struct dhcp_msg &request, const ClientIdentity &client, ReplyKind kind, u32_t now, ReplyBuffer &output)
{
  output.length = 0;

  u8_t message_type;

  switch (kind) {
  case ReplyKind::OFFER:
    message_type = DHCP_OFFER;
    break;
  case ReplyKind::ACK:
  case ReplyKind::INFORM_ACK:
    message_type = DHCP_ACK;
    break;
  case ReplyKind::NAK:
    message_type = DHCP_NAK;
    break;
  default:
    return false;
  }

  u16_t wire_limit = 0;
  bool has_limit = false;

  if (!read_option_value(packet, DHCP_OPTION_MAX_MSG_SIZE, &wire_limit, sizeof(wire_limit), has_limit)) {
    return false;
  }

  u16_t limit = has_limit ? lwip_ntohs(wire_limit) : dhcp_minimum_ip_size;

  if (limit < dhcp_minimum_ip_size) {
    return false;
  }

  if (limit > interface->mtu) {
    limit = interface->mtu;
  }

  if (limit > sizeof(output.data) + IP_HLEN + UDP_HLEN) {
    limit = sizeof(output.data) + IP_HLEN + UDP_HLEN;
  }

  constexpr u16_t minimum_length = DHCP_OPTIONS_OFS + DHCP_MIN_OPTIONS_LEN;

  if (limit < minimum_length + IP_HLEN + UDP_HLEN) {
    return false;
  }

  output.capacity = limit - IP_HLEN - UDP_HLEN;
  memset(output.data, 0, sizeof(output.data));

  struct dhcp_msg response = {};
  response.op = DHCP_BOOTREPLY;
  response.htype = DHCP_HTYPE_ETH;
  response.hlen = ETH_ALEN;
  response.xid = request.xid;
  response.flags = request.flags;
  response.cookie = lwip_htonl(DHCP_MAGIC_COOKIE);

  memcpy(response.chaddr, request.chaddr, sizeof(response.chaddr));

  if (kind != ReplyKind::NAK) {
    ip4_addr_copy(response.ciaddr, request.ciaddr);
  }

  if (kind == ReplyKind::OFFER || kind == ReplyKind::ACK) {
    ip4_addr_copy(response.yiaddr, lease_address);
  }

  memcpy(output.data, &response, DHCP_OPTIONS_OFS);
  output.length = DHCP_OPTIONS_OFS;

  if (!append_option(output, DHCP_OPTION_MESSAGE_TYPE, &message_type, sizeof(message_type))
    || !append_option(output, DHCP_OPTION_SERVER_ID, &netif_ip4_addr(interface)->addr, sizeof(u32_t))) {
    return false;
  }

  if (kind != ReplyKind::NAK && !append_option(output, DHCP_OPTION_SUBNET_MASK, &netif_ip4_netmask(interface)->addr, sizeof(u32_t))) {
    return false;
  }

  if (kind == ReplyKind::OFFER || kind == ReplyKind::ACK) {
    u32_t seconds = lease_seconds;

    if (kind == ReplyKind::OFFER && lease.state == LeaseState::BOUND) {
      const u32_t elapsed = now - lease.started_at_ms;

      if (elapsed >= lease.duration_ms) {
        return false;
      }

      seconds = (lease.duration_ms - elapsed) / 1000;

      if (seconds == 0) {
        return false;
      }
    }

    const u32_t wire_seconds = lwip_htonl(seconds);

    if (!append_option(output, DHCP_OPTION_LEASE_TIME, &wire_seconds, sizeof(wire_seconds))) {
      return false;
    }
  }

  if (client.uses_client_id && !append_option(output, DHCP_OPTION_CLIENT_ID, client.data, client.length)) {
    return false;
  }

  if (output.length == output.capacity) {
    return false;
  }

  output.data[output.length++] = DHCP_OPTION_END;

  if (output.length < minimum_length) {
    output.length = minimum_length;
  }

  return true;
}

static bool send_reply(struct netif *interface, const struct dhcp_msg &request, ReplyKind kind, const ReplyBuffer &encoded)
{
  if (server_pcb == nullptr || interface != server_interface || !netif_is_up(interface) || !netif_is_link_up(interface) || encoded.length == 0 || encoded.length > sizeof(encoded.data)) {
    return false;
  }

  ip4_addr_t destination_ip;

  if (kind == ReplyKind::NAK || ip4_addr_isany_val(request.ciaddr)) {
    ip4_addr_set_u32(&destination_ip, IPADDR_BROADCAST);
  } else {
    ip4_addr_copy(destination_ip, request.ciaddr);
  }

  ip_addr_t destination = {};
  ip_addr_t source = {};
  ip_addr_copy_from_ip4(destination, destination_ip);
  ip_addr_copy_from_ip4(source, *netif_ip4_addr(interface));

  struct pbuf *outgoing = pbuf_alloc(PBUF_TRANSPORT, encoded.length, PBUF_RAM);

  if (!outgoing) {
    return false;
  }

  err_t result = pbuf_take(outgoing, encoded.data, encoded.length);

  if (result == ERR_OK) {
    result = udp_sendto_if_src(server_pcb, outgoing, &destination, DHCP_CLIENT_PORT, interface, &source);
  }

  pbuf_free(outgoing);

  return result == ERR_OK;
}

static void commit_reply(ReplyKind kind, const ClientIdentity &client, u32_t now)
{
  if (kind == ReplyKind::OFFER) {
    if (lease.state == LeaseState::BOUND) {
      return;
    }

    lease.owner = client;
    set_lease_state(LeaseState::OFFERED, now);
  } else if (kind == ReplyKind::ACK) {
    lease.owner = client;
    set_lease_state(LeaseState::BOUND, now);
  }
}

// USB unplug ends the session, but we preserve the bound lease so the same
// client can REQUEST its cached address on replug. That cached lease gets
// replaced in select_reply if a new client connects.
void link_down()
{
  if (lease.state == LeaseState::BOUND) {
    lease_disconnected = true;
  } else if (lease.state != LeaseState::DECLINED) {
    set_lease_state(LeaseState::FREE, sys_now());
  }
}

static void receive_packet(void *context, struct udp_pcb *pcb, struct pbuf *packet, const ip_addr_t *address, u16_t port)
{
  ReplyKind reply = ReplyKind::NONE;
  u32_t now = 0;
  static ClientIdentity client = {};
  static ReplyBuffer encoded = {};
  RequestKind kind = RequestKind::IGNORE;
  struct dhcp_msg header = {};
  RequestOptions options = {};

  (void)pcb;
  (void)address;

  if (!packet) {
    return;
  }

  NETWORK_DEBUG("DHCP receive bytes=%x port=%x\n",
    static_cast<unsigned>(packet->tot_len),
    static_cast<unsigned>(port));

  auto *interface = static_cast<struct netif *>(context);

  if (ip_current_input_netif() != interface || !netif_is_up(interface) || !netif_is_link_up(interface) || port != DHCP_CLIENT_PORT) {
    goto exit;
  }

  if (!read_header(packet, header)) {
    goto exit;
  }

  if (!read_request_options(packet, options)) {
    goto exit;
  }

  kind = classify_request(interface, header, options);
  if (kind == RequestKind::IGNORE) {
    goto exit;
  }

  if (!read_client_identity(packet, header, client)) {
    goto exit;
  }

  now = sys_now();
  expire_lease(now);
  
  reply = select_reply(interface, kind, header, options, client, now);
  NETWORK_DEBUG("DHCP type=%x kind=%x lease=%x reply=%x\n",
    static_cast<unsigned>(options.message_type),
    static_cast<unsigned>(kind),
    static_cast<unsigned>(lease.state),
    static_cast<unsigned>(reply));
  if (reply == ReplyKind::NONE) {
    goto exit;
  }

  if (!build_reply(packet, interface, header, client, reply, now, encoded)) {
    goto exit;
  }

  if (!send_reply(interface, header, reply, encoded)) {
    goto exit;
  }

  commit_reply(reply, client, sys_now());

exit:
  pbuf_free(packet);
}

// lwIP-only teardown for failed startup. Unlike link_down(), this removes
// the PCB and lease, don't use it for an ordinary USB disconnect.
void stop()
{
  if (server_pcb != nullptr) {
    udp_remove(server_pcb);
    server_pcb = nullptr;
  }
  server_interface = nullptr;
  lease = {};
  lease_address = {};
  lease_disconnected = false;
}

bool begin(struct netif *interface)
{
  if (!interface) {
    return false;
  }

  if (server_pcb) {
    return server_interface == interface;
  }

  struct udp_pcb *candidate = udp_new_ip_type(IPADDR_TYPE_V4);

  if (!candidate) {
    return false;
  }

  ip_set_option(candidate, SOF_BROADCAST);

  if (udp_bind(candidate, IP_ADDR_ANY, DHCP_SERVER_PORT) != ERR_OK) {
    udp_remove(candidate);
    return false;
  }

  IP4_ADDR(&lease_address, 192, 168, 7, 2);
  set_lease_state(LeaseState::FREE, sys_now());

  server_pcb = candidate;
  server_interface = interface;

  udp_recv(server_pcb, receive_packet, interface);

  return true;
}

}