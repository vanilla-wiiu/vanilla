#ifndef USB_ASIX_DEF_H
#define USB_ASIX_DEF_H

#include "usb_common_def.h"

#include "asix_registers.h"

// Configuration descriptor + interface descriptor + interrupt IN endpoint + bulk IN endpoint + bulk OUT endpoint
static constexpr uint16_t ax88772_configuration_descriptor_size = 9 + 9 + 7 + 7 + 7;

static uint8_t ax88772_device_descriptor[] USB_DMA_ALIGNED = {
    USB_LEN_DEV_DESC,       // bLength
    USB_DESC_TYPE_DEVICE,   // bDescriptorType
    0x00, 0x02,             // bcdUSB (2.00)
    0xFF,                   // bDeviceClass (vendor-specific)
    0xFF,                   // bDeviceSubClass
    0x00,                   // bDeviceProtocol
    USB_MAX_EP0_SIZE,       // bMaxPacketSize0
    0xAC, 0x05,             // idVendor
    0x02, 0x14,             // idProduct
    0x01, 0x00,             // bcdDevice (0.01)
    0x01,                   // iManufacturer
    0x02,                   // iProduct
    0x03,                   // iSerialNumber
    0x01                    // bNumConfigurations
};
static_assert(sizeof(ax88772_device_descriptor) == USB_LEN_DEV_DESC, "Incorrect device descriptor size");

static uint8_t ax88772_high_speed_configuration[] USB_DMA_ALIGNED = {
    // Configuration descriptor (9 bytes)
    USB_LEN_CFG_DESC,
    USB_DESC_TYPE_CONFIGURATION,
    USB_LOW_BYTE(ax88772_configuration_descriptor_size),
    USB_HIGH_BYTE(ax88772_configuration_descriptor_size),
    0x01,                   // bNumInterfaces
    0x01,                   // bConfigurationValue
    0x00,                   // iConfiguration
    0x80,                   // bmAttributes (bus-powered)
    0x7D,                   // bMaxPower (250mA)
    
    // Interface descriptor (9 bytes)
    USB_LEN_IF_DESC,
    USB_DESC_TYPE_INTERFACE,
    0x00,                   // bInterfaceNumber
    0x00,                   // bAlternateSetting
    0x03,
    0xFF,
    0xFF,
    0x00,
    0x00,

    // Interrupt IN endpoint (7 bytes)
    USB_LEN_EP_DESC,
    USB_DESC_TYPE_ENDPOINT,
    interrupt_in_ep,
    0x03,                   // bmAttributes (interrupt)
    USB_LOW_BYTE(interrupt_mps),
    USB_HIGH_BYTE(interrupt_mps),
    0x07,                   // bInterval (8ms)

    // Bulk IN endpoint (7 bytes)
    USB_LEN_EP_DESC,
    USB_DESC_TYPE_ENDPOINT,
    bulk_in_ep,
    0x02,                   // bmAttributes (bulk)
    USB_LOW_BYTE(high_speed_bulk_mps),
    USB_HIGH_BYTE(high_speed_bulk_mps),
    0x00,                   // bInterval (unused for bulk)

    // Bulk OUT endpoint (7 bytes)
    USB_LEN_EP_DESC,
    USB_DESC_TYPE_ENDPOINT,
    bulk_out_ep,
    0x02,
    USB_LOW_BYTE(high_speed_bulk_mps),
    USB_HIGH_BYTE(high_speed_bulk_mps),
    0x00
};
static_assert(sizeof(ax88772_high_speed_configuration) == ax88772_configuration_descriptor_size, "Incorrect configuration descriptor size");

static uint8_t ax88772_device_qualifier[] USB_DMA_ALIGNED = {
    USB_LEN_DEV_QUALIFIER_DESC,
    USB_DESC_TYPE_DEVICE_QUALIFIER,
    0x00, 0x02,                 // bcdUSB
    0xFF,                       // bDeviceClass
    0xFF,                       // bDeviceSubClass
    0x00,                       // bDeviceProtocol
    USB_MAX_EP0_SIZE,           // bMaxPacketSize0
    0x01,                       // bNumConfigurations
    0x00                        // Reserved
};
static_assert(sizeof(ax88772_device_qualifier) == USB_LEN_DEV_QUALIFIER_DESC, "Incorrect device qualifier size");

// String index 1 - "Apple Inc."
static uint8_t ax88772_manufacturer_descriptor[] USB_DMA_ALIGNED = {
    22,
    USB_DESC_TYPE_STRING,
    'A', 0,
    'p', 0,
    'p', 0,
    'l', 0,
    'e', 0,
    ' ', 0,
    'I', 0,
    'n', 0,
    'c', 0,
    '.', 0,
};
static_assert(sizeof(ax88772_manufacturer_descriptor) == 22, "Incorrect manufacturer descriptor size");

// String index 2 - "Apple USB Ethernet Adapter"
static uint8_t ax88772_product_descriptor[] USB_DMA_ALIGNED = {
    54,
    USB_DESC_TYPE_STRING,
    'A', 0, 'p', 0, 'p', 0, 'l', 0, 'e', 0, 
    ' ', 0,
    'U', 0, 'S', 0, 'B', 0,
    ' ', 0,
    'E', 0, 't', 0, 'h', 0, 'e', 0, 'r', 0, 'n', 0, 'e', 0, 't', 0,
    ' ', 0,
    'A', 0, 'd', 0, 'a', 0, 'p', 0, 't', 0, 'e', 0, 'r', 0
};
static_assert(sizeof(ax88772_product_descriptor) == 54, "Incorrect product descriptor size");

#endif // USB_ASIX_DEF_H