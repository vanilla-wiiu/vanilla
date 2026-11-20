#include "ethernet_backend.h"

#include <string.h>

extern "C" {
#include <FreeRTOS.h>
#include "semphr.h"
#include "lwip/err.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "netif/etharp.h"
#include "task.h"
#include "lwip/pbuf.h"
}

#include "dhcp_server.h"
#include "ethernet_definitions.h"
#include "usb_network.h"
#include "vanilla_pipe.h"

namespace ethernet {

static constexpr UBaseType_t transmit_frame_count = 16;

struct TransmitRing {
  TransmitFrame *slots[transmit_frame_count];
  UBaseType_t head;
  UBaseType_t tail;
  volatile UBaseType_t count;
};

static TransmitFrame *transmit_frames = nullptr;
static void *transmit_frame_storage = nullptr;

static TransmitRing free_transmit_ring = {};
static TransmitRing ready_transmit_ring = {};

static volatile bool transmit_enabled = false;
static volatile uint32_t transmit_generation = 0;

static bool receive_enabled = false;

static struct netif usb_netif = {};

// The USB worker asks lwIP to apply link changes. Both tasks can read/update
// these flags. Volatile makes the quick checks read memory each time (it cannot
// stop another task changing a flag between reads). service_link_state() checks
// again inside a critical section before deciding whether to queue an update.
static volatile bool link_updates_ready = false;
static volatile bool requested_link_up = false;
static volatile bool applied_link_up = false;
static volatile bool link_update_pending = false;

static SemaphoreHandle_t initialization_done = nullptr;
static bool initialization_succeeded = false;

static const uint8_t device_mac[ETH_ALEN] = {
  0x02, 0x00, 0x00, 0x00, 0x00, 0x02
};

static uint32_t input_submitted_count = 0;
static uint32_t input_dropped_count = 0;

static volatile bool lease_reset_requested = false;

static volatile TransmitStatistics transmit_statistics = {};

static uint32_t transmit_batch_depth = 0;
static TransmitFrame *pending_transmit_frame = nullptr;
static uint32_t pending_transmit_generation = 0;

static void notify_transmit()
{
  usb_network_notify_transmit();
}

static bool push_transmit_frame(TransmitRing &ring, TransmitFrame *frame)
{
  const UBaseType_t count = ring.count;
  if (count == transmit_frame_count) {
    return false;
  }
  ring.slots[ring.tail] = frame;
  ring.tail++;
  if (ring.tail == transmit_frame_count) {
    ring.tail = 0;
  }
  ring.count = count + 1;
  return true;
}

static TransmitFrame *pop_transmit_frame(TransmitRing &ring)
{
  const UBaseType_t count = ring.count;
  if (count == 0) {
    return nullptr;
  }
  TransmitFrame *frame = ring.slots[ring.head];
  ring.head++;
  if (ring.head == transmit_frame_count) {
    ring.head = 0;
  }
  ring.count = count - 1;
  return frame;
}

bool is_ready()
{
  return link_updates_ready;
}

static void apply_link_state(void *context)
{
  (void)context;

  bool up;
  bool reset_lease;

  taskENTER_CRITICAL();
  up = requested_link_up;
  reset_lease = lease_reset_requested;
  lease_reset_requested = false;
  taskEXIT_CRITICAL();

  if (reset_lease) {
    dhcp::link_down();
    vanilla_pipe::link_down();
  }

  if (up) {
    netif_set_link_up(&usb_netif);
  } else {
    netif_set_link_down(&usb_netif);
  }

  taskENTER_CRITICAL();
  applied_link_up = up;
  link_update_pending = false;
  taskEXIT_CRITICAL();

  service_link_state();
}

void service_link_state()
{
  if (!link_updates_ready
    || link_update_pending
    || (requested_link_up == applied_link_up && !lease_reset_requested)) {
    return;
  }

  bool should_post;

  taskENTER_CRITICAL();

  should_post =
    link_updates_ready &&
    !link_update_pending &&
    (requested_link_up != applied_link_up || lease_reset_requested);

  if (should_post) {
    link_update_pending = true;
  }

  taskEXIT_CRITICAL();

  if (should_post && tcpip_callback_with_block(apply_link_state, nullptr, 0) != ERR_OK) {
    taskENTER_CRITICAL();
    link_update_pending = false;
    taskEXIT_CRITICAL();
  }
}

void request_link_state(bool up)
{
  taskENTER_CRITICAL();

  if (requested_link_up && !up) {
    lease_reset_requested = true;
  }
  requested_link_up = up;

  taskEXIT_CRITICAL();

  service_link_state();
}

void set_receive_enabled(bool enabled)
{
  // USB control/data callbacks and the worker already hold the USB state lock
  receive_enabled = enabled;
}

bool take_transmit_frame(TransmitFrame *&frame)
{
  frame = nullptr;

  if (!transmit_enabled || ready_transmit_ring.count == 0) {
    return false;
  }

  taskENTER_CRITICAL();
  if (transmit_enabled) {
    frame = pop_transmit_frame(ready_transmit_ring);
  }
  taskEXIT_CRITICAL();

  return frame != nullptr;
}

// Return a buffer to the free ring after its owner has finished using it.
// USB must complete or stop its transfer before returning an active buffer.
void release_transmit_frame(TransmitFrame *frame)
{
  if (frame == nullptr) {
    return;
  }

  taskENTER_CRITICAL();
  const bool returned = push_transmit_frame(free_transmit_ring, frame);
  taskEXIT_CRITICAL();

  configASSERT(returned);
  (void)returned;
}

// When stopping TX, return waiting buffers to the free queue. lwIP may still
// be filling another buffer, and USB may still be sending one. Neither can be
// reused yet. Changing transmit_generation tells lwIP to discard its old buffer
// once it finishes copying. USB returns its buffer after completion/teardown.
void set_transmit_enabled(bool enabled)
{
  taskENTER_CRITICAL();
  if (transmit_enabled && !enabled) {
    ++transmit_generation;
  }
  transmit_enabled = enabled;

  if (!enabled) {
    while (ready_transmit_ring.count != 0) {
      TransmitFrame *frame = pop_transmit_frame(ready_transmit_ring);
      const bool returned = push_transmit_frame(free_transmit_ring, frame);
      configASSERT(returned);
      (void)returned;
    }
  }

  taskEXIT_CRITICAL();
}

// Return lwIP's unfinished buffer to the free queue after a failed or cancelled
// send. Only lwIP may do this - a USB reset can interrupt copying, and reusing
// the same memory then would let two tasks write into it. The reset changes
// transmit_generation, lwIP notices the changed number and releases the buffer.
static void discard_pending_transmit()
{
  TransmitFrame *frame = pending_transmit_frame;
  pending_transmit_frame = nullptr;
  release_transmit_frame(frame);
}

// Put the completed buffer in the ready queue so USB can take it. First check
// its saved transmit_generation - TX may have stopped/restarted during copying.
// Check and queue inside one critical section so a reset cannot slip between
// those steps. After queueing, lwIP must stop using the buffer. Notify USB only
// after leaving the critical section.
static err_t flush_pending_transmit()
{
  TransmitFrame *frame = pending_transmit_frame;
  if (frame == nullptr) {
    return ERR_OK;
  }
  pending_transmit_frame = nullptr;

  // The USB driver reads memory four bytes at a time. Zero any extra bytes
  // it may read past the end of our data - these bytes are not sent to the
  // host. Frames with ASIX headers/padding use an even number of bytes,
  // so at most two more bytes need clearing for the final four-byte read.
  const uint16_t word_padding = (4U - (frame->length & 3U)) & 3U;
  memset(frame->data + frame->length, 0, word_padding);

  taskENTER_CRITICAL();
  bool queued = false;
  if (transmit_enabled && pending_transmit_generation == transmit_generation) {
    queued = push_transmit_frame(ready_transmit_ring, frame);
    configASSERT(queued);
    if (queued) {
      transmit_statistics.queued += frame->frame_count;
    }
  }
  taskEXIT_CRITICAL();

  if (!queued) {
    release_transmit_frame(frame);
    return ERR_IF;
  }

  notify_transmit();
  return ERR_OK;
}

TransmitBatch::TransmitBatch()
{
  transmit_batch_depth++;
}

TransmitBatch::~TransmitBatch()
{
  configASSERT(transmit_batch_depth != 0);
  if (--transmit_batch_depth == 0) {
    flush_pending_transmit();
  }
}

// Copy one Ethernet frame from lwIP into a USB buffer, with its ASIX header
// and any padding
static err_t queue_transmit_frame(struct pbuf *packet)
{
  // Discard any buffer left over from before TX stopped
  const bool enabled = transmit_enabled && transmit_frames != nullptr;
  const uint32_t generation = transmit_generation;

  if (pending_transmit_frame != nullptr &&
      (!enabled || pending_transmit_generation != generation)) {
    discard_pending_transmit();
  }
  if (!enabled) {
    return ERR_IF;
  }
  if (packet == nullptr || packet->tot_len < ETH_HLEN || packet->tot_len > ETH_FRAME_LEN) {
    return ERR_BUF;
  }

  const uint16_t record_length = ax88772::frame_header_size +
      packet->tot_len + (packet->tot_len & 1U);

  if (pending_transmit_frame != nullptr &&
      pending_transmit_frame->length + record_length > transmit_buffer_size) {
    if (flush_pending_transmit() != ERR_OK) {
      return ERR_IF;
    }
  }

  if (pending_transmit_frame == nullptr) {
    taskENTER_CRITICAL();
    TransmitFrame *reserved = nullptr;
    if (transmit_enabled) {
      reserved = pop_transmit_frame(free_transmit_ring);
      if (reserved == nullptr) {
        ++transmit_statistics.pool_empty;
      }
    }
    const bool still_enabled = transmit_enabled;
    pending_transmit_generation = transmit_generation;
    taskEXIT_CRITICAL();

    if (reserved == nullptr) {
      return still_enabled ? ERR_MEM : ERR_IF;
    }
    reserved->length = 0;
    reserved->frame_count = 0;
    pending_transmit_frame = reserved;
  }

  TransmitFrame *frame = pending_transmit_frame;
  uint8_t *record = frame->data + frame->length;
  const uint16_t frame_length = packet->tot_len;
  if (ax88772::prepare_frame_record(record, sizeof(frame->data) - frame->length,
                                  frame_length) != record_length) {
    if (frame->frame_count == 0) {
      discard_pending_transmit();
    }
    return ERR_BUF;
  }

  if (pbuf_copy_partial(packet, record + ax88772::frame_header_size,
                        frame_length, 0) != frame_length) {
    if (frame->frame_count == 0) {
      discard_pending_transmit();
    }
    return ERR_BUF;
  }

  if (!transmit_enabled || pending_transmit_generation != transmit_generation) {
    discard_pending_transmit();
    return ERR_IF;
  }

  // Record the copied frame's length while lwIP still owns this buffer.
  // USB can take it only after flush_pending_transmit() queues the pointer.
  const uint8_t index = frame->frame_count++;
  frame->frame_lengths[index] = frame_length;
  frame->length += record_length;

  if (transmit_batch_depth == 0 || frame->frame_count == transmit_records_per_buffer) {
    return flush_pending_transmit();
  }
  return ERR_OK;
}

static err_t link_output(struct netif *interface, struct pbuf *packet)
{
  (void)interface;

  return queue_transmit_frame(packet);
}

static err_t initialize_interface(struct netif *interface)
{
  interface->name[0] = 'e';
  interface->name[1] = 'n';

  interface->output = etharp_output;
  interface->linkoutput = link_output;

  // Match the standard 1,500-byte IP MTU (perhaps jumbo frames would be worth
  // looking into one day, but our current system works and will have wide
  // compatibility).
  interface->mtu = ETH_DATA_LEN;
  interface->hwaddr_len = ETH_ALEN;

  memcpy(interface->hwaddr, device_mac, sizeof(device_mac));

  interface->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;

  return ERR_OK;
}

static void setup_interface(void *context)
{
  (void)context;

  ip4_addr_t address;
  ip4_addr_t netmask;
  ip4_addr_t gateway;

  // TODO: Currently hardcoded 192.168.7.0/24. May revise this later.
  IP4_ADDR(&address, 192, 168, 7, 1);
  IP4_ADDR(&netmask, 255, 255, 255, 0);
  IP4_ADDR(&gateway, 0, 0, 0, 0);

  initialization_succeeded = netif_add(
    &usb_netif,
    &address,
    &netmask,
    &gateway,
    nullptr,
    initialize_interface,
    tcpip_input
  ) != nullptr;

  if (initialization_succeeded) {
    if (dhcp::begin(&usb_netif)) {
      netif_set_up(&usb_netif);

      if (!vanilla_pipe::begin(&usb_netif)) {
        printf("Failed to start vanilla_pipe\n");
        netif_set_down(&usb_netif);
        dhcp::stop();
        netif_remove(&usb_netif);
        initialization_succeeded = false;
      }
    } else {
      netif_remove(&usb_netif);
      initialization_succeeded = false;
    }
  }

  xSemaphoreGive(initialization_done);
}

bool begin()
{
  static bool started = false;

  if (started) {
    return true;
  }

  transmit_frame_storage = pvPortMalloc(sizeof(TransmitFrame) * transmit_frame_count + alignof(TransmitFrame) - 1);
  if (!transmit_frame_storage) {
    goto fail;
  }
  // USB buffers use addresses divisible by 32. pvPortMalloc does not guarantee
  // that, so allocate spare bytes and move the usable start forward to such an
  // address. vPortFree() needs the original address returned by pvPortMalloc,
  // so retain it separately from transmit_frames.
  transmit_frames = reinterpret_cast<TransmitFrame *>(
      (reinterpret_cast<uintptr_t>(transmit_frame_storage) + alignof(TransmitFrame) - 1) &
      ~(static_cast<uintptr_t>(alignof(TransmitFrame)) - 1));

  transmit_enabled = false;
  transmit_generation = 0;
  free_transmit_ring = {};
  ready_transmit_ring = {};
  receive_enabled = false;

  for (UBaseType_t i = 0; i < transmit_frame_count; i++) {
    TransmitFrame *transmit_slot = &transmit_frames[i];

    if (!push_transmit_frame(free_transmit_ring, transmit_slot)) {
      goto fail;
    }
  }

  initialization_done = xSemaphoreCreateBinary();

  if (initialization_done == nullptr) {
    goto fail;
  }

  initialization_succeeded = false;

  if (tcpip_callback(setup_interface, nullptr) != ERR_OK) {
    goto fail;
  }

  xSemaphoreTake(initialization_done, portMAX_DELAY);

  vSemaphoreDelete(initialization_done);
  initialization_done = nullptr;

  if (!initialization_succeeded) {
    goto fail;
  }

  taskENTER_CRITICAL();
  link_updates_ready = true;
  taskEXIT_CRITICAL();

  started = true;
  return true;

fail:
  if (initialization_done != nullptr) {
    vSemaphoreDelete(initialization_done);
    initialization_done = nullptr;
  }

  ready_transmit_ring = {};
  free_transmit_ring = {};
  transmit_enabled = false;
  receive_enabled = false;

  if (transmit_frame_storage != nullptr) {
    vPortFree(transmit_frame_storage);
    transmit_frame_storage = nullptr;
    transmit_frames = nullptr;
  }

  return false;
}

bool receive_frame(const uint8_t *data, uint16_t length)
{
  if (!receive_enabled ||
      usb_netif.input == nullptr ||
      data == nullptr ||
      length < ETH_HLEN ||
      length > ETH_FRAME_LEN) {
    input_dropped_count++;
    return false;
  }

  // Copy before the SDK rearms/reuses its OUT buffer. Post the pbuf once
  // to lwIP instead of adding another packet queue or retaining data.
  struct pbuf *packet = pbuf_alloc(PBUF_RAW, length, PBUF_POOL);
  if (packet == nullptr) {
    input_dropped_count++;
    return false;
  }

  if (pbuf_take(packet, data, length) != ERR_OK) {
    pbuf_free(packet);
    input_dropped_count++;
    return false;
  }

  // Queue the packet for lwIP without waiting for queue space since lwIP may
  // itself be waiting to access USB. A successful post leaves lwIP responsible
  // for freeing the pbuf. If posting fails, we must free it here instead.
  if (usb_netif.input(packet, &usb_netif) != ERR_OK) {
    pbuf_free(packet);
    input_dropped_count++;
    return false;
  }

  input_submitted_count++;
  return true;
}

ReceiveStatistics get_receive_statistics()
{
  taskENTER_CRITICAL();
  const ReceiveStatistics snapshot = {input_submitted_count, input_dropped_count};
  taskEXIT_CRITICAL();
  return snapshot;
}

TransmitStatistics get_transmit_statistics()
{
  taskENTER_CRITICAL();
  const TransmitStatistics snapshot = {
    transmit_statistics.queued,
    transmit_statistics.pool_empty
  };
  taskEXIT_CRITICAL();
  return snapshot;
}

}
