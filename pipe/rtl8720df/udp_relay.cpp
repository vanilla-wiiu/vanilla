#include "udp_relay.h"

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
#include "lwip/ip.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
}

#include <stddef.h>

#include "src/diagnostics/debug_log.h"
#include "ethernet_backend.h"
#include "vanilla_ports.h"

namespace udp_relay {

static constexpr uint16_t console_port_offset = 100;

struct RelayChannel {
    uint16_t client_port;
    uint16_t console_port;

    struct udp_pcb *client_port_pcb;
    struct udp_pcb *console_port_pcb;

    ChannelStatistics statistics;
};

static RelayChannel channels[] = {
    { PORT_MSG, PORT_MSG - console_port_offset, nullptr, nullptr, {} },
    { PORT_VID, PORT_VID - console_port_offset, nullptr, nullptr, {} },
    { PORT_AUD, PORT_AUD - console_port_offset, nullptr, nullptr, {} },
    { PORT_HID, PORT_HID - console_port_offset, nullptr, nullptr, {} },
    { PORT_CMD, PORT_CMD - console_port_offset, nullptr, nullptr, {} }
};

static constexpr size_t channel_count = sizeof(channels) / sizeof(channels[0]);

static_assert(channel_count == 5, "Incorrect relay channel count");

static struct netif *usb_interface = nullptr;
static struct netif *wifi_interface = nullptr;

static ip_addr_t client_address = {};
static ip_addr_t console_address = {};

static bool relay_enabled = false;


// Relay callbacks and raw UDP/netif operations run on lwIP thread.
// Reuse the received pbuf during this synchronous send, then let the receive
// callback free it. Extra copies, queues or driver queries here slow Wi-Fi reception.
static err_t forward_packet(struct udp_pcb *sender, struct pbuf *packet, const ip_addr_t &destination, uint16_t port, struct netif *output)
{
    if (!sender || !output ||
        !netif_is_up(output) ||
        !netif_is_link_up(output) ||
        ip4_addr_isany_val(*netif_ip4_addr(output))) {
        return ERR_IF;
    }

    const err_t result = udp_sendto_if(sender, packet, &destination, port, output);

    if (result != ERR_OK) {
        NETWORK_DEBUG("UDP relay send failed=%x\n", static_cast<unsigned>(result));
    }

    return result;
}

static void receive_from_client(void *context, struct udp_pcb *pcb, struct pbuf *packet, const ip_addr_t *address, u16_t port)
{
    (void)port;

    if (packet == nullptr) {
        return;
    }

    auto *channel = static_cast<RelayChannel *>(context);

    if (relay_enabled &&
        channel != nullptr && address != nullptr &&
        pcb == channel->console_port_pcb &&
        ip_current_input_netif() == usb_interface &&
        ip_addr_cmp(address, &client_address)) {
        channel->statistics.client_packets++;
        const err_t result = forward_packet(
            channel->client_port_pcb,
            packet,
            console_address,
            channel->console_port,
            wifi_interface
        );
        if (result != ERR_OK) {
            channel->statistics.client_send_errors++;
            channel->statistics.last_client_error = result;
        }
    }

    pbuf_free(packet);
}

static void receive_from_console(void *context, struct udp_pcb *pcb, struct pbuf *packet, const ip_addr_t *address, u16_t port)
{
    (void)port;

    if (!packet) {
        return;
    }

    auto *channel = static_cast<RelayChannel *>(context);

    if (relay_enabled &&
        channel && address &&
        pcb == channel->client_port_pcb &&
        ip_current_input_netif() == wifi_interface &&
        ip_addr_cmp(address, &console_address)) {
        channel->statistics.console_packets++;

        // forward_packet() may split a large UDP packet into smaller IP
        // packets before returning. Use a TransmitBatch to buffer them
        // together for reduced USB overhead.
        ethernet::TransmitBatch batch;

        const err_t result = forward_packet(
            channel->console_port_pcb,
            packet,
            client_address,
            channel->client_port,
            usb_interface
        );
        if (result != ERR_OK) {
            channel->statistics.console_send_errors++;
            channel->statistics.last_console_error = result;
        }
    }

    pbuf_free(packet);
}

static void close_channels()
{
    relay_enabled = false;

    for (auto &channel : channels) {
        if (channel.client_port_pcb) {
            udp_remove(channel.client_port_pcb);
            channel.client_port_pcb = nullptr;
        }

        if (channel.console_port_pcb) {
            udp_remove(channel.console_port_pcb);
            channel.console_port_pcb = nullptr;
        }
    }
}

static bool open_endpoint(struct udp_pcb *&pcb, uint16_t port, udp_recv_fn callback, RelayChannel *channel)
{
    pcb = udp_new_ip_type(IPADDR_TYPE_V4);
    if (!pcb) {
        return false;
    }

    if (udp_bind(pcb, IP_ADDR_ANY, port) != ERR_OK) {
        udp_remove(pcb);
        pcb = nullptr;
        return false;
    }

    udp_recv(pcb, callback, channel);

    return true;
}

bool begin(struct netif *usb, struct netif *wifi)
{
    if (!usb || !wifi || usb == wifi) {
        return false;
    }

    if (usb_interface) {
        return usb_interface == usb && wifi_interface == wifi;
    }

    for (auto &channel : channels) {
        if (!open_endpoint(channel.client_port_pcb, channel.client_port, receive_from_console, &channel) ||
            !open_endpoint(channel.console_port_pcb, channel.console_port, receive_from_client, &channel)) {
            close_channels();
            return false;
        }
    }

    usb_interface = usb;
    wifi_interface = wifi;

    client_address = {};

    IP_ADDR4(&console_address, 192, 168, 1, 10);

    relay_enabled = false;

    return true;
}

void set_session(const ip_addr_t &client, bool enabled)
{
    const bool next_enabled = enabled &&
        usb_interface != nullptr &&
        wifi_interface != nullptr &&
        IP_IS_V4(&client) && !ip_addr_isany(&client);

    taskENTER_CRITICAL();
    client_address = client;
    relay_enabled = next_enabled;
    taskEXIT_CRITICAL();

}

Statistics get_statistics()
{
    Statistics snapshot = {};
    taskENTER_CRITICAL();
    for (size_t i = 0; i < channel_count; ++i) {
        snapshot.channels[i] = channels[i].statistics;
        snapshot.channels[i].port = channels[i].client_port;
    }
    taskEXIT_CRITICAL();
    return snapshot;
}

}
