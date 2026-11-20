#ifndef USB_NETWORK_DIAGNOSTICS_H
#define USB_NETWORK_DIAGNOSTICS_H

#include <stdint.h>
#include "../../build_config.h"

#if VANILLA_DIAGNOSTICS
struct UsbDiagnostics {
    uint32_t setup_calls;
    uint32_t bulk_state;
    uint32_t irq_enabled;
    uint32_t global_status;
    uint32_t global_mask;
    uint32_t ahb_config;
    uint32_t device_control;
    uint32_t bulk_in_control;
    uint32_t bulk_in_size;
    uint32_t bulk_in_interrupts;
    uint32_t bulk_in_length;
    uint32_t bulk_in_count;
    uint32_t last_setup;
    uint32_t last_setup_value;
    uint32_t last_setup_index;
    uint32_t last_setup_length;
    uint32_t ep0_state;
    uint32_t ep0_in_control;
    uint32_t ep0_in_size;
    uint32_t ep0_in_interrupts;
    uint32_t ep0_in_length;
    uint32_t ep0_in_count;
    uint32_t ep0_out_control;
    uint32_t ep0_out_size;
    uint32_t ep0_out_interrupts;
    uint32_t ep0_out_length;
    uint32_t ep0_out_count;
    uint32_t endpoint_interrupts;
    uint32_t endpoint_mask;
    uint32_t in_interrupt_mask;
    uint32_t out_interrupt_mask;
    uint32_t nonperiodic_tx_status;
};

UsbDiagnostics usb_network_diagnostics();
#endif

struct UsbTransferStatistics {
    uint32_t window_ticks;
    uint32_t data_completions;
    uint32_t completed_bytes;
    uint32_t completed_frames;
    uint32_t zlp_completions;
    uint32_t fifo_rearms;
};

// Returns and clears performance window (preserves cumulative diagnostics)
UsbTransferStatistics usb_network_take_transfer_statistics();

#endif // USB_NETWORK_DIAGNOSTICS_H
