#include <Arduino.h>
#include <wifi_conf.h>
#include "../../build_config.h"

extern "C" {
#include "FreeRTOS.h"
}

#include "serial_monitor.h"
#include "../../ethernet_backend.h"
#include "../../usb_network.h"
#include "../../udp_relay.h"
#include "wifi_diagnostics.h"

template <typename T>
static void print_stat(const char *name, T value, int base = DEC)
{
    Serial.print(name);
    Serial.print('=');
    Serial.println(value, base);
}

static void print_ethernet_statistics()
{
    const auto tx = ethernet::get_transmit_statistics();
    const auto rx = ethernet::get_receive_statistics();
    print_stat("tx_queued", tx.queued);
    print_stat("tx_pool_empty", tx.pool_empty);
    print_stat("usb_out_submitted", rx.submitted);
    print_stat("usb_out_dropped", rx.dropped);
}

static void print_usb_performance()
{
    const auto perf = usb_network_take_transfer_statistics();
    print_stat("uptime_ms", millis());
    print_stat("usb_tick_us", 1000000UL / configTICK_RATE_HZ);
    print_stat("usb_window_ticks", perf.window_ticks);
    print_stat("usb_data_completions", perf.data_completions);
    print_stat("usb_completed_bytes", perf.completed_bytes);
    print_stat("usb_completed_frames", perf.completed_frames);
    print_stat("usb_zlp_completions", perf.zlp_completions);
    print_stat("usb_fifo_rearms", perf.fifo_rearms);
    print_ethernet_statistics();
}

static void print_relay_statistics()
{
    const auto relay = udp_relay::get_statistics();
    print_stat("uptime_ms", millis());
    Serial.println("relay_columns=port,console_rx,console_errors,last_console_error,client_rx,client_errors,last_client_error");
    for (const auto &channel : relay.channels) {
        Serial.print("relay=");
        Serial.print(channel.port);
        Serial.print(',');
        Serial.print(channel.console_packets);
        Serial.print(',');
        Serial.print(channel.console_send_errors);
        Serial.print(',');
        Serial.print(channel.last_console_error);
        Serial.print(',');
        Serial.print(channel.client_packets);
        Serial.print(',');
        Serial.print(channel.client_send_errors);
        Serial.print(',');
        Serial.println(channel.last_client_error);
    }
}

static void print_wifi_statistics()
{
#if VANILLA_DIAGNOSTICS
    const auto events = wifi_diagnostics::get_statistics();
    print_stat("wifi_connect_events", events.connect_events);
    print_stat("wifi_disconnect_events", events.disconnect_events);
    print_stat("wifi_last_connect_tick", events.last_connect_tick);
    print_stat("wifi_last_disconnect_tick", events.last_disconnect_tick);
    print_stat("wifi_last_disconnect_reason", events.last_disconnect_reason);
#else
    Serial.println("wifi_events=unavailable");
#endif
    int rssi = 0;
    const int rssi_status = wifi_get_rssi(&rssi);
    print_stat("wifi_rssi_status", rssi_status);
    if (rssi_status == RTW_SUCCESS) {
        print_stat("wifi_rssi", rssi);
    }
    // Despite its int* signature, the SDK writes only one byte here. Zero
    // the whole int first so its upper bytes cannot contain stale values.
    // Keep driver queries on demand; do not move this into packet callbacks.
    int channel = 0;
    const int channel_status = wifi_get_channel(&channel);
    print_stat("wifi_channel_status", channel_status);
    if (channel_status == RTW_SUCCESS) {
        print_stat("wifi_channel", channel);
    }
}

#if VANILLA_DIAGNOSTICS
static void print_usb_snapshot(const UsbDiagnostics &usb)
{
    print_stat("usb_setup_calls", usb.setup_calls);
    print_stat("usb_bulk_state", usb.bulk_state);
    print_stat("usb_irq_enabled", usb.irq_enabled);
    print_stat("usb_global_status", usb.global_status, HEX);
    print_stat("usb_global_mask", usb.global_mask, HEX);
    print_stat("usb_ahb_config", usb.ahb_config, HEX);
    print_stat("usb_device_control", usb.device_control, HEX);
    print_stat("usb_bulk_in_control", usb.bulk_in_control, HEX);
    print_stat("usb_bulk_in_size", usb.bulk_in_size, HEX);
    print_stat("usb_bulk_in_interrupts", usb.bulk_in_interrupts, HEX);
    print_stat("usb_bulk_in_length", usb.bulk_in_length);
    print_stat("usb_bulk_in_count", usb.bulk_in_count);
    print_stat("usb_last_setup", usb.last_setup, HEX);
    print_stat("usb_last_setup_value", usb.last_setup_value, HEX);
    print_stat("usb_last_setup_index", usb.last_setup_index, HEX);
    print_stat("usb_last_setup_length", usb.last_setup_length, HEX);
    print_stat("usb_ep0_state", usb.ep0_state, HEX);
    print_stat("usb_ep0_in_control", usb.ep0_in_control, HEX);
    print_stat("usb_ep0_in_size", usb.ep0_in_size, HEX);
    print_stat("usb_ep0_in_interrupts", usb.ep0_in_interrupts, HEX);
    print_stat("usb_ep0_in_length", usb.ep0_in_length, HEX);
    print_stat("usb_ep0_in_count", usb.ep0_in_count, HEX);
    print_stat("usb_ep0_out_control", usb.ep0_out_control, HEX);
    print_stat("usb_ep0_out_size", usb.ep0_out_size, HEX);
    print_stat("usb_ep0_out_interrupts", usb.ep0_out_interrupts, HEX);
    print_stat("usb_ep0_out_length", usb.ep0_out_length, HEX);
    print_stat("usb_ep0_out_count", usb.ep0_out_count, HEX);
    print_stat("usb_endpoint_interrupts", usb.endpoint_interrupts, HEX);
    print_stat("usb_endpoint_mask", usb.endpoint_mask, HEX);
    print_stat("usb_in_interrupt_mask", usb.in_interrupt_mask, HEX);
    print_stat("usb_out_interrupt_mask", usb.out_interrupt_mask, HEX);
    print_stat("usb_nonperiodic_tx_status", usb.nonperiodic_tx_status, HEX);
}
#endif

void serial_monitor::service()
{
    const int command = Serial.available() > 0 ? Serial.read() : -1;
    switch (command) {
    case 'h': {
#if VANILLA_DIAGNOSTICS
        const auto usb = usb_network_diagnostics();
#endif
        print_stat("uptime_ms", millis());
        print_stat("heap_free", static_cast<unsigned long>(xPortGetFreeHeapSize()));
        print_stat("heap_min", static_cast<unsigned long>(xPortGetMinimumEverFreeHeapSize()));
        print_stat("usb_passes", usb_network_service_pass_count());
        print_ethernet_statistics();
#if VANILLA_DIAGNOSTICS
        print_usb_snapshot(usb);
#else
        Serial.println("usb_details=unavailable");
#endif
        break;
    }
    case 'p':
        print_usb_performance();
        break;
    case 'r':
        print_relay_statistics();
        break;
    case 'w':
        print_wifi_statistics();
        break;
    default:
        break;
    }
}
