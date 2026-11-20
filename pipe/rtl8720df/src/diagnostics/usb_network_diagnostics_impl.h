#if VANILLA_DIAGNOSTICS
UsbDiagnostics usb_network_diagnostics()
{
    UsbDiagnostics snapshot = {};
    snapshot.ep0_state = 0xFFFFFFFFU;

    taskENTER_CRITICAL();

    snapshot.setup_calls = usb_setup_calls;
    snapshot.bulk_state = static_cast<uint32_t>(bulk_in_state);
    snapshot.irq_enabled = NVIC_GetEnableIRQ(USB_OTG_IRQ);
    snapshot.global_status = USB_GLOBAL->GINTSTS;
    snapshot.global_mask = USB_GLOBAL->GINTMSK;
    snapshot.ahb_config = USB_GLOBAL->GAHBCFG;
    snapshot.device_control = USB_DEVICE->DCTL;
    snapshot.bulk_in_control = USB_INEP(bulk_in_ep & 0x7F)->DIEPCTL;
    snapshot.bulk_in_size = USB_INEP(bulk_in_ep & 0x7F)->DIEPTSIZ;
    snapshot.bulk_in_interrupts = USB_INEP(bulk_in_ep & 0x7F)->DIEPINT;

    snapshot.last_setup = (static_cast<uint32_t>(usb_last_setup.bmRequestType) << 8) |
                          usb_last_setup.bRequest;
    snapshot.last_setup_value = usb_last_setup.wValue;
    snapshot.last_setup_index = usb_last_setup.wIndex;
    snapshot.last_setup_length = usb_last_setup.wLength;

    if (configured_device != nullptr) {
        snapshot.ep0_state = configured_device->ep0_state;

        const auto *pcd = static_cast<const usbd_pcd_t *>(configured_device->pcd);
        if (pcd != nullptr) {
            snapshot.ep0_in_length = pcd->in_ep[0].xfer_len;
            snapshot.ep0_in_count = pcd->in_ep[0].xfer_count;
            snapshot.ep0_out_length = pcd->out_ep[0].xfer_len;
            snapshot.ep0_out_count = pcd->out_ep[0].xfer_count;
            snapshot.bulk_in_length = pcd->in_ep[bulk_in_ep & 0x7F].xfer_len;
            snapshot.bulk_in_count = pcd->in_ep[bulk_in_ep & 0x7F].xfer_count;
        }
    }

    // Read status only; do not acknowledge interrupts or change endpoint state.
    snapshot.ep0_in_control = USB_INEP(0)->DIEPCTL;
    snapshot.ep0_in_size = USB_INEP(0)->DIEPTSIZ;
    snapshot.ep0_in_interrupts = USB_INEP(0)->DIEPINT;
    snapshot.ep0_out_control = USB_OUTEP(0)->DOEPCTL;
    snapshot.ep0_out_size = USB_OUTEP(0)->DOEPTSIZ;
    snapshot.ep0_out_interrupts = USB_OUTEP(0)->DOEPINT;
    snapshot.endpoint_interrupts = USB_DEVICE->DAINT;
    snapshot.endpoint_mask = USB_DEVICE->DAINTMSK;
    snapshot.in_interrupt_mask = USB_DEVICE->DIEPMSK;
    snapshot.out_interrupt_mask = USB_DEVICE->DOEPMSK;
    snapshot.nonperiodic_tx_status = USB_GLOBAL->GNPTXSTS;

    taskEXIT_CRITICAL();

    return snapshot;
}
#endif // VANILLA_DIAGNOSTICS
