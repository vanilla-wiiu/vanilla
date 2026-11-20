#include "usb_network.h"

#include <string.h>

extern "C" {
#include "usbd.h"
#include "usbd_core.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "usbd_hal.h"
#include "rtl8721d_usb.h"
}

#include "ax88772_protocol.h"
#include "asix_registers.h"
#include "src/diagnostics/debug_log.h"
#include "ethernet_backend.h"
#include "mii_registers.h"

#include "usb_asix_def.h"

// Define USB task priorities. Doesn't affect NVIC interrupt priorities.
// Retune only with streaming and EP0 tests.
static constexpr UBaseType_t usb_worker_priority = configMAX_PRIORITIES - 2;
static constexpr UBaseType_t usb_isr_priority = configMAX_PRIORITIES - 1;

static volatile uint32_t usb_service_passes = 0;
#if VANILLA_DIAGNOSTICS
static volatile uint32_t usb_setup_calls = 0;
static usb_setup_req_t usb_last_setup = {};
#endif

static ax88772::LinkStatus interrupt_buffer USB_DMA_ALIGNED = {};
static ax88772::FrameAssembler bulk_out_assembler = {};

static ax88772::Registers asix_registers = {
    { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 },
    AX_DEFAULT_RX_CTL & ~AX_RX_CTL_SO,
    AX88772_MEDIUM_DEFAULT,
    {
        AX88772_IPG0_DEFAULT,
        AX88772_IPG1_DEFAULT,
        AX88772_IPG2_DEFAULT
    },
    0,
    0,
    false,
    AX_PHYSEL_PSEL | AX_PHYSEL_SSEN,
    AX_SWRESET_CLEAR,
    ax88772::make_phy_registers(),
    { 0 }
};

struct EndpointConfiguration {
    uint8_t address;
    uint8_t type;
    uint16_t max_packet_size;
};

static const EndpointConfiguration endpoint_configurations[] = {
    { bulk_in_ep, USB_CH_EP_TYPE_BULK, high_speed_bulk_mps },
    { bulk_out_ep, USB_CH_EP_TYPE_BULK, high_speed_bulk_mps },
    { interrupt_in_ep, USB_CH_EP_TYPE_INTR, interrupt_mps }
};

static constexpr uint8_t endpoint_count = sizeof(endpoint_configurations) / sizeof(endpoint_configurations[0]);

static usb_dev_t *configured_device = nullptr;
static uint8_t opened_endpoint_count = 0;

static bool bulk_out_data_enabled = false;
static bool bulk_in_data_enabled = false;
static bool backend_link_requested_up = false;

// The worker checks the link/data state frequently. Reuse the last result until
// a host register write sets data_path_dirty, or the device's configured/backend
// ready state changes. UsbStateLock protects the cached result and its inputs.
// Also revisit disabled data paths that still have partial/unsent data to discard.
static bool data_path_dirty = true;
static bool data_path_configured = false;
static bool data_path_backend_ready = false;

static constexpr uint16_t bulk_out_buffer_size = 2048;
static uint8_t bulk_out_buffer[bulk_out_buffer_size] USB_DMA_ALIGNED;

static bool bulk_out_receive_armed = false;
static bool interrupt_in_busy = false;
static bool asix_link_seen_up = false;
static bool asix_have_completed_link_status = false;
static bool asix_last_completed_link_up = false;

static SemaphoreHandle_t usb_state_mutex = nullptr;

static bool discard_bulk_out_until_end = false;

static uint8_t control_buffer[USB_MAX_EP0_SIZE] USB_DMA_ALIGNED;

struct PendingControlWrite {
    uint8_t command;
    uint16_t phy_address;
    uint16_t reg;
    uint16_t length;
};

enum class BulkInState {
    IDLE,
    DATA_READY,
    DATA_IN_FLIGHT,
    ZLP_READY,
    ZLP_IN_FLIGHT
};

static bool service_bulk_in();

static uint8_t bulk_in_zlp_buffer[4] USB_DMA_ALIGNED;
static uint8_t *bulk_in_buffer = bulk_in_zlp_buffer;
static ethernet::TransmitFrame *bulk_in_frame = nullptr;
static uint16_t bulk_in_length = 0;
static uint16_t bulk_in_frame_count = 0;
static constexpr uint8_t bulk_in_batch_frame_limit = 16;
static BulkInState bulk_in_state = BulkInState::IDLE;

static UsbTransferStatistics usb_transfer_statistics = {};
static TickType_t usb_statistics_started_tick = 0;

static TaskHandle_t usb_service_task_handle = nullptr;

static PendingControlWrite pending_control_write = {};

// iOS sends this unknown request to us and expects us to acknowledge
static constexpr uint8_t UNKNOWN_APPLE_INIT_REQUEST = 0x26;
static constexpr uint16_t UNKNOWN_APPLE_INIT_VALUE = 0x003F;

// Even though we only offer Ethernet over high-speed (USB 2.0), we still expose
// a basic full-speed (USB 1.1) interface (control only) so that the descriptors
// remain valid at either speed (as required by the USB 2.0 spec)
static uint8_t control_only_configuration[] USB_DMA_ALIGNED = {
    // Configuration descriptor
    USB_LEN_CFG_DESC,
    USB_DESC_TYPE_CONFIGURATION,
    18, 0,                      // wTotalLength
    0x01,                       // bNumInterfaces
    0x01,                       // bConfigurationValue
    0x00,                       // iConfiguration
    0x80,                       // bmAttributes
    0x7D,                       // bMaxPower

    USB_LEN_IF_DESC,
    USB_DESC_TYPE_INTERFACE,
    0x00,                       // bInterfaceNumber,
    0x00,                       // bAlternateSetting
    0x00,                       // bNumEndpoints
    0xFF,                       // bInterfaceClass
    0xFF,                       // bInterfaceSubClass
    0x00,                       // bInterfaceProtocol
    0x00                        // bInterface
};
static_assert(sizeof(control_only_configuration) == 18, "Incorrect control-only configuration size");

static uint8_t other_speed_configuration[ax88772_configuration_descriptor_size] USB_DMA_ALIGNED;

// Locking scope class for various operations
class UsbStateLock {
public:
    explicit UsbStateLock(TickType_t timeout = portMAX_DELAY)
        : acquired(xSemaphoreTakeRecursive(usb_state_mutex, timeout) == pdTRUE)
    {
    }

    ~UsbStateLock()
    {
        if (acquired) {
            xSemaphoreGiveRecursive(usb_state_mutex);
        }
    }

    bool is_locked() const
    {
        return acquired;
    }

    UsbStateLock(const UsbStateLock &) = delete;
    UsbStateLock &operator=(const UsbStateLock &) = delete;

private:
    bool acquired;
};

static void invalidate_asix_link_status()
{
    asix_have_completed_link_status = false;
}

static uint8_t ep0_data_in(usb_dev_t *device)
{
    (void)device;
    return HAL_OK;
}

static uint8_t *select_high_speed_configuration(uint16_t &length)
{
    length = sizeof(ax88772_high_speed_configuration);
    return ax88772_high_speed_configuration;
}

static uint8_t *get_descriptor(usb_setup_req_t *request, usbd_speed_type_t speed, uint16_t *length)
{
    UsbStateLock lock;

    pending_control_write = {};

    NETWORK_DEBUG("USB desc value=%x index=%x length=%x speed=%x\n",
       static_cast<unsigned>(request->wValue),
       static_cast<unsigned>(request->wIndex),
       static_cast<unsigned>(request->wLength),
       static_cast<unsigned>(speed));

    *length = 0;

    const uint8_t type = request->wValue >> 8;
    const uint8_t index = request->wValue & 0xFF;

    switch (type) {
    case USB_DESC_TYPE_DEVICE:
        if (index != 0 || request->wIndex != 0) {
            return nullptr;
        }

        *length = sizeof(ax88772_device_descriptor);
        return ax88772_device_descriptor;

    case USB_DESC_TYPE_CONFIGURATION:
        if (index != 0 || request->wIndex != 0) {
            return nullptr;
        }

        if (speed == USBD_SPEED_HIGH) {
            return select_high_speed_configuration(*length);
        }

        if (speed == USBD_SPEED_FULL) {
            *length = sizeof(control_only_configuration);
            return control_only_configuration;
        }

        return nullptr;

    case USB_DESC_TYPE_STRING:
        if (index == 0) {
            *length = sizeof(language_descriptor);
            return language_descriptor;
        }

        if (request->wIndex != language_id) {
            return nullptr;
        }

        switch (index) {
        case 1:
            *length = sizeof(ax88772_manufacturer_descriptor);
            return ax88772_manufacturer_descriptor;
        case 2:
            *length = sizeof(ax88772_product_descriptor);
            return ax88772_product_descriptor;
        case 3:
            *length = sizeof(serial_descriptor);
            return serial_descriptor;
        default:
            return nullptr;
        }

    case USB_DESC_TYPE_DEVICE_QUALIFIER:
        if (index != 0 || request->wIndex != 0) {
            return nullptr;
        }

        *length = sizeof(ax88772_device_qualifier);
        return ax88772_device_qualifier;

    case USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION:
        if (index != 0 || request->wIndex != 0) {
            return nullptr;
        }

        if (speed == USBD_SPEED_HIGH) {
            *length = sizeof(control_only_configuration);
            memcpy(other_speed_configuration, control_only_configuration, *length);
        } else if (speed == USBD_SPEED_FULL) {
            const uint8_t *configuration = select_high_speed_configuration(*length);
            memcpy(other_speed_configuration, configuration, *length);
        } else {
            return nullptr;
        }

        other_speed_configuration[1] = USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION;

        return other_speed_configuration;

    default:
        return nullptr;
    }
}

static void reset_bulk_out_decoder()
{
    ax88772::reset_frame_assembler(bulk_out_assembler);
    discard_bulk_out_until_end = false;
}

static void reject_bulk_out_transfer()
{
    reset_bulk_out_decoder();
    discard_bulk_out_until_end = true;
}

static void release_bulk_in_frame()
{
    if (bulk_in_frame != nullptr) {
        ethernet::release_transmit_frame(bulk_in_frame);
        bulk_in_frame = nullptr;
    }
    bulk_in_buffer = bulk_in_zlp_buffer;
}

static bool update_data_path()
{
    const bool configured = configured_device != nullptr &&
                            opened_endpoint_count == endpoint_count &&
                            configured_device->dev_speed == USBD_SPEED_HIGH &&
                            configured_device->dev_state == USBD_STATE_CONFIGURED;
    const bool backend_ready = configured && ethernet::is_ready();

    if (!data_path_dirty && configured == data_path_configured &&
        backend_ready == data_path_backend_ready &&
        (bulk_out_data_enabled || bulk_out_assembler.header_used == 0) &&
        (bulk_in_data_enabled || bulk_in_state != BulkInState::DATA_READY)) {
        return backend_link_requested_up;
    }

    data_path_dirty = false;
    data_path_configured = configured;
    data_path_backend_ready = backend_ready;

    bool link_up = false;
    bool out_enabled = false;
    bool in_enabled = false;

    // PHY link must not depend on MAC receive-enable bits. Hosts can wait
    // for the link notification before enabling reception or starting DHCP.
    link_up = configured && ax88772::phy_link_up(asix_registers, backend_ready);
    const bool mac_running = link_up && (asix_registers.receive_control & AX_RX_CTL_SO);
    out_enabled = mac_running && !(asix_registers.software_reset & AX_SWRESET_RT);
    in_enabled = mac_running && (asix_registers.medium_mode & AX_MEDIUM_RE) && !(asix_registers.software_reset & AX_SWRESET_RR);

    if (!out_enabled && bulk_out_assembler.header_used != 0) {
        reset_bulk_out_decoder();
        discard_bulk_out_until_end = true;
    }

    if (!in_enabled && bulk_in_state == BulkInState::DATA_READY) {
        release_bulk_in_frame();
        bulk_in_state = BulkInState::IDLE;
        bulk_in_length = 0;
        bulk_in_frame_count = 0;
    }

    if ((out_enabled != bulk_out_data_enabled ||
        in_enabled != bulk_in_data_enabled)) {
        NETWORK_DEBUG("ASIX data out=%x in=%x rx=%x medium=%x\n",
            static_cast<unsigned>(out_enabled),
            static_cast<unsigned>(in_enabled),
            static_cast<unsigned>(asix_registers.receive_control),
            static_cast<unsigned>(asix_registers.medium_mode));
    }

    // Keep backend state updates off the unchanged-state path
    if (out_enabled != bulk_out_data_enabled) {
        ethernet::set_receive_enabled(out_enabled);
        bulk_out_data_enabled = out_enabled;
    }
    if (in_enabled != bulk_in_data_enabled) {
        ethernet::set_transmit_enabled(in_enabled);
        bulk_in_data_enabled = in_enabled;
    }
    if (link_up != backend_link_requested_up) {
        ethernet::request_link_state(link_up);
        backend_link_requested_up = link_up;
    }

    return link_up;
}

static void process_bulk_out_data(const uint8_t *data, uint16_t length)
{
    const bool end_of_transfer = length < sizeof(bulk_out_buffer);

    if (!bulk_out_data_enabled) {
        ax88772::reset_frame_assembler(bulk_out_assembler);
        discard_bulk_out_until_end = !end_of_transfer;
        return;
    }

    uint16_t offset = 0;

    while (offset < length && !discard_bulk_out_until_end) {
        const uint16_t remaining = length - offset;

        if (bulk_out_assembler.header_used == 0 &&
            remaining >= ax88772::frame_header_size) {
            uint16_t frame_length = 0;
            if (ax88772::read_frame_header(data + offset, remaining, frame_length) &&
                frame_length >= ETH_HLEN && frame_length <= ETH_FRAME_LEN &&
                frame_length <= remaining - ax88772::frame_header_size) {
                ethernet::receive_frame(data + offset + ax88772::frame_header_size,
                                           frame_length);
                offset += ax88772::frame_header_size + frame_length;
                continue;
            }
        }

        uint16_t consumed = 0;

        const ax88772::FrameAssemblyResult result = ax88772::assemble_frame(
            bulk_out_assembler,
            data + offset,
            remaining,
            consumed
        );

        if (consumed == 0 || consumed > remaining) {
            reject_bulk_out_transfer();
            break;
        }

        offset += consumed;

        switch (result) {
        case ax88772::FrameAssemblyResult::NEED_MORE:
            break;
        case ax88772::FrameAssemblyResult::FRAME_READY:
            ethernet::receive_frame(bulk_out_assembler.frame, bulk_out_assembler.frame_length);

            ax88772::reset_frame_assembler(bulk_out_assembler);
            break;
        case ax88772::FrameAssemblyResult::PADDING:
            ax88772::reset_frame_assembler(bulk_out_assembler);
            break;
        case ax88772::FrameAssemblyResult::INVALID:
            reject_bulk_out_transfer();
            break;
        }
    }

    if (end_of_transfer) {
        if (!discard_bulk_out_until_end && bulk_out_assembler.header_used != 0) {
            // USB transfer ended with incomplete header or payload
            reject_bulk_out_transfer();
        }

        reset_bulk_out_decoder();
    }
}

static uint8_t arm_bulk_out_receive()
{
    if (configured_device == nullptr || opened_endpoint_count != endpoint_count) {
        return HAL_ERR_HW;
    }

    if (bulk_out_receive_armed) {
        return HAL_OK;
    }

    const uint8_t status = usbd_ep_receive(configured_device, bulk_out_ep, bulk_out_buffer, sizeof(bulk_out_buffer));

    if (status == HAL_OK) {
        bulk_out_receive_armed = true;
    }

    return status;
}

static void close_endpoints()
{
    data_path_dirty = true;
    ethernet::set_receive_enabled(false);
    ethernet::set_transmit_enabled(false);
    ethernet::request_link_state(false);

    bulk_out_data_enabled = false;
    bulk_in_data_enabled = false;
    backend_link_requested_up = false;

    pending_control_write = {};
    bulk_out_receive_armed = false;

    reset_bulk_out_decoder();

    while (opened_endpoint_count != 0) {
        opened_endpoint_count--;
        usbd_ep_deinit(configured_device, endpoint_configurations[opened_endpoint_count].address);
    }

    release_bulk_in_frame();
    bulk_in_state = BulkInState::IDLE;
    bulk_in_length = 0;
    bulk_in_frame_count = 0;

    interrupt_in_busy = false;
    asix_link_seen_up = false;
    asix_have_completed_link_status = false;
    configured_device = nullptr;
}

static uint8_t set_configuration(usb_dev_t *device, uint8_t configuration)
{
    NETWORK_DEBUG("USB set configuration=%x speed=%x\n",
       static_cast<unsigned>(configuration),
       static_cast<unsigned>(device->dev_speed));

    UsbStateLock lock;

    if (configuration != 1) {
        return HAL_ERR_HW;
    }

    if (device->dev_speed != USBD_SPEED_HIGH && device->dev_speed != USBD_SPEED_FULL) {
        return HAL_ERR_HW;
    }

    close_endpoints();
    configured_device = device;

    if (device->dev_speed == USBD_SPEED_FULL) {
        return HAL_OK;
    }

    while (opened_endpoint_count < endpoint_count) {
        const EndpointConfiguration &endpoint = endpoint_configurations[opened_endpoint_count];
        const uint8_t status = usbd_ep_init(device, endpoint.address, endpoint.type, endpoint.max_packet_size);
        if (status != HAL_OK) {
            close_endpoints();
            return status;
        }

        opened_endpoint_count++;
    }

    const uint8_t status = arm_bulk_out_receive();

    if (status != HAL_OK) {
        close_endpoints();
        return status;
    }

    return HAL_OK;
}

static uint8_t clear_configuration(usb_dev_t *device, uint8_t configuration)
{
    (void)device;
    (void)configuration;

    NETWORK_DEBUG("USB clear configuration=%x\n", static_cast<unsigned>(configuration));

    UsbStateLock lock;

    close_endpoints();

    asix_registers.receive_control &= ~AX_RX_CTL_SO;

    return HAL_OK;
}

static uint8_t data_out(usb_dev_t *device, uint8_t endpoint_number, uint16_t length)
{
    UsbStateLock lock;

    if ((endpoint_number & 0x7F) != (bulk_out_ep & 0x7F)) {
        return HAL_OK;
    }

    if (device != configured_device || opened_endpoint_count != endpoint_count || !bulk_out_receive_armed) {
        return HAL_OK;
    }

    bulk_out_receive_armed = false;

    if (length > sizeof(bulk_out_buffer)) {
        reject_bulk_out_transfer();
        return arm_bulk_out_receive();
    }

    update_data_path();

    process_bulk_out_data(bulk_out_buffer, length);

    return arm_bulk_out_receive();
}

static void write_le16(uint8_t *data, uint16_t value)
{
    data[0] = value & 0xFFU;
    data[1] = value >> 8;
}

static uint8_t handle_vendor_read(usb_dev_t *device, usb_setup_req_t *request)
{
    const uint8_t vendor_in_request = USB_D2H | USB_REQ_TYPE_VENDOR | USB_REQ_RECIPIENT_DEVICE;

    if (request->bmRequestType != vendor_in_request || request->wLength == 0) {
        return HAL_ERR_HW;
    }

    if (opened_endpoint_count != endpoint_count) {
        return HAL_ERR_HW;
    }

    const bool addressed_read = request->bRequest == AX_CMD_READ_MII_REG || request->bRequest == AX_CMD_READ_EEPROM;
    if (!addressed_read && (request->wValue != 0 || request->wIndex != 0)) {
        return HAL_ERR_HW;
    }

    uint16_t response_length = 0;

    switch (request->bRequest) {
    case AX_CMD_STATMNGSTS_REG:
        if (request->wValue != 0 || request->wIndex != 0) {
            return HAL_ERR_HW;
        }

        control_buffer[0] = AX_AX88772A_CHIPCODE;
        if (asix_registers.software_mii_enabled) {
            control_buffer[0] |= AX_HOST_EN;
        }

        response_length = 1;
        break;
    case AX_CMD_READ_PHY_ID:
        if (request->wValue != 0 || request->wIndex != 0) {
            return HAL_ERR_HW;
        }

        control_buffer[0] = AX_EMBD_PHY_ADDR;
        control_buffer[1] = AX_EMBD_PHY_ADDR;

        response_length = 2;
        break;
    case AX_CMD_READ_MII_REG:
    {
        if (request->wValue > 0x1F || request->wIndex > 0x1F) {
            return HAL_ERR_HW;
        }

        const uint16_t value = request->wValue == AX_EMBD_PHY_ADDR
            ? ax88772::read_phy_register(asix_registers, request->wIndex, ethernet::is_ready())
            : 0xFFFFU; // No PHY responds at this address

        write_le16(control_buffer, value);
        response_length = 2;
        break;
    }
    case AX_CMD_READ_NODE_ID:
        memcpy(control_buffer, asix_registers.mac_address, sizeof(asix_registers.mac_address));
        response_length = sizeof(asix_registers.mac_address);
        break;
    case AX_CMD_READ_RX_CTL:
        write_le16(control_buffer, asix_registers.receive_control);
        response_length = 2;
        break;
    case AX_CMD_READ_MEDIUM_STATUS:
        write_le16(control_buffer, asix_registers.medium_mode);
        response_length = 2;
        break;
    case AX_CMD_READ_IPG012:
        memcpy(control_buffer, asix_registers.ipg, sizeof(asix_registers.ipg));
        response_length = sizeof(asix_registers.ipg);
        break;
    case AX_CMD_READ_MONITOR_MODE:
        control_buffer[0] = asix_registers.monitor_mode;

        if (device->dev_speed == USBD_SPEED_HIGH) {
            control_buffer[0] |= AX_MONITOR_HSFS;
        }

        response_length = 1;
        break;
    case AX_CMD_READ_GPIOS:
        control_buffer[0] = asix_registers.gpio_state;
        response_length = 1;
        break;
    case AX_CMD_READ_EEPROM:
        if (request->wIndex != 0 || request->wValue >= AX_EEPROM_LEN / sizeof(uint16_t)) {
            return HAL_ERR_HW;
        }

        // Simulate a fake initial EEPROM (all readable words blank)
        memset(control_buffer, 0xFF, 2);
        response_length = 2;
        break;
    case AX_CMD_READ_MULTI_FILTER:
        memcpy(control_buffer, asix_registers.multicast_filter, sizeof(asix_registers.multicast_filter));
        response_length = sizeof(asix_registers.multicast_filter);
        break;
    case AX_CMD_SW_PHY_STATUS:
        control_buffer[0] = asix_registers.phy_selection;
        response_length = 1;
        break;
    default:
        return HAL_ERR_HW;
    }

    if (response_length > request->wLength) {
        response_length = request->wLength;
    }

    return usbd_ep0_transmit(device, control_buffer, response_length);
}

static uint8_t handle_vendor_write(usb_dev_t *device, usb_setup_req_t *request)
{
    const uint8_t vendor_out_request = USB_H2D | USB_REQ_TYPE_VENDOR | USB_REQ_RECIPIENT_DEVICE;

    if (request->bmRequestType != vendor_out_request) {
        return HAL_ERR_HW;
    }

    if (opened_endpoint_count != endpoint_count) {
        return HAL_ERR_HW;
    }

    PendingControlWrite pending = {};

    if (request->bRequest == AX_CMD_WRITE_MII_REG) {
        if (request->wValue > 0x1F ||
            request->wIndex > 0x1F ||
            request->wLength != sizeof(uint16_t)) {
            return HAL_ERR_HW;
        }

        pending.command = request->bRequest;
        pending.phy_address = request->wValue;
        pending.reg = request->wIndex;
        pending.length = sizeof(uint16_t);
    } else if (request->bRequest == AX_CMD_WRITE_NODE_ID || request->bRequest == AX_CMD_WRITE_MULTI_FILTER) {
        if (request->wValue != 0 || request->wIndex != 0) {
            return HAL_ERR_HW;
        }

        const uint16_t length = request->bRequest == AX_CMD_WRITE_NODE_ID ? sizeof(asix_registers.mac_address) : sizeof(asix_registers.multicast_filter);

        if (request->wLength != length) {
            return HAL_ERR_HW;
        }

        pending.command = request->bRequest;
        pending.length = length;
    }

    if (pending.length != 0) {
        pending_control_write = pending;

        const uint8_t status = usbd_ep0_receive(device, control_buffer, pending.length);
        if (status != HAL_OK) {
            pending_control_write = {};
        }

        return status;
    }

    if (request->wLength != 0) {
        return HAL_ERR_HW;
    }

    if (request->bRequest != AX_CMD_WRITE_IPG0 && request->wIndex != 0) {
        return HAL_ERR_HW;
    }

    switch (request->bRequest) {
    case AX_CMD_SET_SW_MII:
        if (request->wValue != 0) {
            return HAL_ERR_HW;
        }

        asix_registers.software_mii_enabled = true;
        break;
    case AX_CMD_SET_HW_MII:
        if (request->wValue != 0) {
            return HAL_ERR_HW;
        }

        asix_registers.software_mii_enabled = false;
        break;
    case AX_CMD_WRITE_RX_CTL:
        asix_registers.receive_control = request->wValue;
        data_path_dirty = true;
        break;
    case AX_CMD_WRITE_MEDIUM_MODE:
        asix_registers.medium_mode = request->wValue;
        data_path_dirty = true;
        break;
    case AX_CMD_WRITE_MONITOR_MODE:
        if (request->wValue > 0xFFU) {
            return HAL_ERR_HW;
        }

        // Only store bits that are writable
        asix_registers.monitor_mode = request->wValue & (AX_MONITOR_MODE | AX_MONITOR_LINK | AX_MONITOR_MAGIC);
        break;
    case AX_CMD_WRITE_GPIOS:
        if (request->wValue > 0xFFU) {
            return HAL_ERR_HW;
        }

        asix_registers.gpio_state = request->wValue;
        break;
    case AX_CMD_WRITE_IPG0:
        if (request->wIndex > 0xFFU) {
            return HAL_ERR_HW;
        }

        asix_registers.ipg[0] = request->wValue & 0xFFU;
        asix_registers.ipg[1] = request->wValue >> 8;
        asix_registers.ipg[2] = request->wIndex & 0xFFU;
        break;
    case AX_CMD_SW_PHY_SELECT:
        if (request->wValue > 0xFFU) {
            return HAL_ERR_HW;
        }

        asix_registers.phy_selection = request->wValue;
        data_path_dirty = true;
        break;
    case AX_CMD_SW_RESET:
        if (request->wValue > 0xFFU) {
            return HAL_ERR_HW;
        }

        asix_registers.software_reset = request->wValue;
        data_path_dirty = true;

        if (!(asix_registers.software_reset & AX_SWRESET_IPRL) || (asix_registers.software_reset & AX_SWRESET_IPPD)) {
            asix_registers.phy = ax88772::make_phy_registers();
            invalidate_asix_link_status();
        }
        break;
    case UNKNOWN_APPLE_INIT_REQUEST:
        if (request->wValue != UNKNOWN_APPLE_INIT_VALUE) {
            return HAL_ERR_HW;
        }
        break;
    default:
        return HAL_ERR_HW;
    }

    update_data_path();

    return usbd_ep0_transmit_status(device);
}

static uint8_t ep0_data_out(usb_dev_t *device)
{
    UsbStateLock lock;

    const PendingControlWrite pending = pending_control_write;
    pending_control_write = {};

    if (pending.length == 0 ||
        device != configured_device ||
        device->dev_state != USBD_STATE_CONFIGURED ||
        opened_endpoint_count != endpoint_count) {
        return HAL_ERR_HW;
    }

    if (usbd_core_get_rx_data_size(device, 0) != pending.length) {
        return HAL_ERR_HW;
    }

    switch (pending.command) {
    case AX_CMD_WRITE_MII_REG:
    {
        const uint16_t value = (control_buffer[0] & 0xFF) | (control_buffer[1] & 0xFF) << 8;

        if (pending.phy_address == AX_EMBD_PHY_ADDR) {
            const bool phy_operating = (asix_registers.software_reset & AX_SWRESET_IPRL) && !(asix_registers.software_reset & AX_SWRESET_IPPD);

            if (phy_operating && pending.reg == MII_BMCR && (value & BMCR_RESET)) {
                invalidate_asix_link_status();
            }

            ax88772::write_phy_register(asix_registers, pending.reg, value);
            data_path_dirty = true;
        }
        break;
    }
    case AX_CMD_WRITE_NODE_ID:
        memcpy(asix_registers.mac_address, control_buffer, sizeof(asix_registers.mac_address));
        break;
    case AX_CMD_WRITE_MULTI_FILTER:
        memcpy(asix_registers.multicast_filter, control_buffer, sizeof(asix_registers.multicast_filter));
        break;
    default:
        return HAL_ERR_HW;
    }

    update_data_path();

    return HAL_OK;
}

static uint8_t handle_setup(usb_dev_t *device, usb_setup_req_t *request)
{
#if VANILLA_DIAGNOSTICS
    taskENTER_CRITICAL();
    usb_last_setup = *request;
    ++usb_setup_calls;
    taskEXIT_CRITICAL();
#endif

    NETWORK_DEBUG("USB setup type=%x request=%x value=%x index=%x length=%x\n",
       static_cast<unsigned>(request->bmRequestType),
       static_cast<unsigned>(request->bRequest),
       static_cast<unsigned>(request->wValue),
       static_cast<unsigned>(request->wIndex),
       static_cast<unsigned>(request->wLength));

    UsbStateLock lock;

    pending_control_write = {};

    if (device != configured_device || device->dev_state != USBD_STATE_CONFIGURED) {
        return HAL_ERR_HW;
    }

    const uint8_t request_type = request->bmRequestType & USB_REQ_TYPE_MASK;

    if (request_type == USB_REQ_TYPE_CLASS) {
        return HAL_ERR_HW;
    }

    if (request_type == USB_REQ_TYPE_VENDOR) {
        if (request->bmRequestType & USB_D2H) {
            return handle_vendor_read(device, request);
        } else {
            return handle_vendor_write(device, request);
        }
    }

    if (request_type != USB_REQ_TYPE_STANDARD) {
        return HAL_ERR_HW;
    }

    if (request->wIndex != 0 || request->wValue != 0) {
        return HAL_ERR_HW;
    }

    const uint8_t interface_in_request = USB_D2H | USB_REQ_RECIPIENT_INTERFACE;

    switch (request->bRequest) {
    case USB_REQ_GET_INTERFACE:
        if (request->bmRequestType != interface_in_request || request->wLength != 1) {
            return HAL_ERR_HW;
        }

        control_buffer[0] = 0;

        return usbd_ep0_transmit(device, control_buffer, 1);

    case USB_REQ_SET_INTERFACE:
        // Only alternate setting zero exists
        // USB 2.0 section 9.4.10 permits STALL here
        return HAL_ERR_HW;

    case USB_REQ_GET_STATUS:
        if (request->bmRequestType != interface_in_request || request->wLength != 2) {
            return HAL_ERR_HW;
        }

        control_buffer[0] = 0;
        control_buffer[1] = 0;

        return usbd_ep0_transmit(device, control_buffer, 2);

    default:
        return HAL_ERR_HW;
    }
}

static uint8_t data_in(usb_dev_t *device, uint8_t endpoint_number)
{
    UsbStateLock lock;

    if (device != configured_device || opened_endpoint_count != endpoint_count) {
        return HAL_OK;
    }

    const uint8_t number = endpoint_number & 0x7F;

    if (number == (interrupt_in_ep & 0x7F)) {
        if (!interrupt_in_busy) {
            return HAL_OK;
        }

        interrupt_in_busy = false;

        const bool current_link_up = ax88772::phy_link_up(asix_registers, ethernet::is_ready());
        const ax88772::LinkStatus current_status = ax88772::make_link_status(current_link_up);

        asix_last_completed_link_up = (interrupt_buffer.bytes[2] & BIT(0)) != 0;
        asix_have_completed_link_status = memcmp(interrupt_buffer.bytes, current_status.bytes, sizeof(interrupt_buffer.bytes)) == 0;
    } else if (number == (bulk_in_ep & 0x7F)) {
        if (bulk_in_state != BulkInState::DATA_IN_FLIGHT && bulk_in_state != BulkInState::ZLP_IN_FLIGHT) {
            return HAL_OK;
        }

        if (bulk_in_state == BulkInState::DATA_IN_FLIGHT) {
            ++usb_transfer_statistics.data_completions;
            usb_transfer_statistics.completed_bytes += bulk_in_length;
            usb_transfer_statistics.completed_frames += bulk_in_frame_count;

            release_bulk_in_frame();
        } else {
            ++usb_transfer_statistics.zlp_completions;
        }

        NETWORK_DEBUG("USB bulk complete state=%x len=%x\n",
            static_cast<unsigned>(bulk_in_state),
            static_cast<unsigned>(bulk_in_length));

        switch (bulk_in_state) {
        case BulkInState::DATA_IN_FLIGHT:
            if (bulk_in_length % high_speed_bulk_mps == 0) {
                bulk_in_state = BulkInState::ZLP_READY;
            } else {
                bulk_in_state = BulkInState::IDLE;
            }
            break;
        case BulkInState::ZLP_IN_FLIGHT:
            bulk_in_state = BulkInState::IDLE;
            break;
        default:
            break;
        }

        const bool submitted = service_bulk_in();
        if (!submitted &&
            (bulk_in_state == BulkInState::DATA_READY ||
             bulk_in_state == BulkInState::ZLP_READY) &&
            usb_service_task_handle != nullptr) {
            xTaskNotifyGive(usb_service_task_handle);
        }
        return HAL_OK;
    }

    if (usb_service_task_handle != nullptr) {
        xTaskNotifyGive(usb_service_task_handle);
    }

    return HAL_OK;
}

static bool prepare_bulk_in_batch()
{
    bulk_in_buffer = bulk_in_zlp_buffer;
    bulk_in_length = 0;
    bulk_in_frame_count = 0;

    for (uint8_t examined = 0; examined < bulk_in_batch_frame_limit; ++examined) {
        ethernet::TransmitFrame *frame = nullptr;
        if (!ethernet::take_transmit_frame(frame)) {
            break;
        }

        uint16_t offset = 0;
        uint16_t accepted_length = 0;
        uint8_t accepted_count = 0;
        for (uint8_t index = 0; index < frame->frame_count; ++index) {
            const uint16_t frame_length = frame->frame_lengths[index];
            const uint16_t record_length = ax88772::frame_header_size +
                frame_length + (frame_length & 1U);

            if (ax88772::accepts_destination(asix_registers,
                    frame->data + offset + ax88772::frame_header_size, frame_length)) {
                if (offset != accepted_length) {
                    // An earlier frame was rejected - close the gap by moving
                    // this frame with its ASIX header and padding (if all
                    // frames are accepted, send the buffer without moving it)
                    memmove(frame->data + accepted_length, frame->data + offset, record_length);
                }
                accepted_length += record_length;
                ++accepted_count;

            }
            offset += record_length;
        }

        if (accepted_count == 0) {
            ethernet::release_transmit_frame(frame);
            continue;
        }

        if (accepted_length != frame->length) {
            const uint16_t word_padding = (4U - (accepted_length & 3U)) & 3U;
            memset(frame->data + accepted_length, 0, word_padding);
        }

        bulk_in_frame = frame;
        bulk_in_buffer = frame->data;
        bulk_in_length = accepted_length;
        bulk_in_frame_count = accepted_count;
        return true;
    }

    // Send available frames immediately - never wait to fill a batch
    return false;
}

static bool service_bulk_in()
{
    if (configured_device == nullptr || opened_endpoint_count != endpoint_count ||
        configured_device->dev_state != USBD_STATE_CONFIGURED) {
        return false;
    }

    if (bulk_in_state == BulkInState::IDLE) {
        if (!bulk_in_data_enabled) {
            return false;
        }

        if (!prepare_bulk_in_batch()) {
            return false;
        }

        bulk_in_state = BulkInState::DATA_READY;
    }

    const BulkInState retry_state = bulk_in_state;
    uint16_t length = bulk_in_length;

    switch (bulk_in_state) {
    case BulkInState::DATA_READY:
        bulk_in_state = BulkInState::DATA_IN_FLIGHT;
        break;
    case BulkInState::ZLP_READY:
        length = 0;
        bulk_in_state = BulkInState::ZLP_IN_FLIGHT;
        break;
    default:
        return false;
    }

    const uint8_t status = usbd_ep_transmit(configured_device, bulk_in_ep, bulk_in_buffer, length);

    NETWORK_DEBUG("USB bulk submit len=%x status=%x interrupt_busy=%x\n",
        static_cast<unsigned>(length),
        static_cast<unsigned>(status),
        static_cast<unsigned>(interrupt_in_busy));

    if (status != HAL_OK) {
        bulk_in_state = retry_state;
        return false;
    }

    return true;
}

// Run only from usb_service_task. Do not queue an initial down notification, or
// replace interrupt_buffer while it is in flight. Completion records what
// the host actually received. A reset can invalidate that completed state.
static void service_interrupt_in(bool link_up)
{

    if (!asix_link_seen_up && !link_up) {
        return;
    }

    if (link_up) {
        asix_link_seen_up = true;
    }

    if (asix_have_completed_link_status && link_up == asix_last_completed_link_up) {
        return;
    }

    if (interrupt_in_busy) {
        return;
    }

    interrupt_buffer = ax88772::make_link_status(link_up);

    usbd_pcd_t *pcd = static_cast<usbd_pcd_t *>(configured_device->pcd);

    usb_os_spinlock_safe(pcd->lock);
    USB_INEP(interrupt_in_ep & 0x7F)->DIEPINT = USB_OTG_DIEPINT_ITTXFE;
    usb_os_spinunlock_safe(pcd->lock);

    interrupt_in_busy = true;

    const uint8_t status = usbd_ep_transmit(configured_device, interrupt_in_ep, interrupt_buffer.bytes, sizeof(interrupt_buffer));

    if (status != HAL_OK) {
        interrupt_in_busy = false;
        return;
    }
}

// A submitted link notification can remain in RAM instead of reaching the USB
// hardware's transmit buffer (FIFO). If the host asks for it while that FIFO
// is empty, copy the untouched eight-byte notification into it under the PCD
// lock. FIFO zero is shared with control/bulk traffic - flushing it could throw
// away their data too. Leave global NAK state and unrelated interrupts alone.
static void service_interrupt_fifo()
{
    if (configured_device == nullptr || configured_device->pcd == nullptr || !interrupt_in_busy) {
        return;
    }

    usbd_pcd_t *pcd = static_cast<usbd_pcd_t *>(configured_device->pcd);
    const uint8_t number = interrupt_in_ep & 0x7F;

    // Skip locking unless the host has asked for data while the FIFO is empty.
    // The SDK task can fill it before we acquire the lock, so check again below
    // before copying anything (sending the same bytes twice would corrupt it)
    if ((USB_INEP(number)->DIEPINT & USB_OTG_DIEPINT_ITTXFE) == 0) {
        return;
    }

    usb_os_spinlock_safe(pcd->lock);
    usbd_pcd_ep_t &ep = pcd->in_ep[number];

    const uint32_t packet_words = (interrupt_mps + 3U) / 4U;
    const uint32_t tx_status = USB_GLOBAL->GNPTXSTS;

    if (pcd->config.intr_use_ptx_fifo == 0 &&
        pcd->config.dma_enable == 0 &&
        ep.type == USB_CH_EP_TYPE_INTR &&
        ep.tx_fifo_num == 0 &&
        ep.xfer_len == interrupt_mps && ep.xfer_count == 0 &&
        ep.xfer_buff != nullptr &&
        (USB_INEP(number)->DIEPINT & USB_OTG_DIEPINT_ITTXFE) != 0 &&
        (tx_status & USB_OTG_GNPTXSTS_NPTQXSAV) != 0 &&
        (tx_status & USB_OTG_GNPTXSTS_NPTXFSAV) > packet_words) {
        const uint16_t length = static_cast<uint16_t>(ep.xfer_len);
        if (usb_hal_write_packet(ep.xfer_buff, number, length) == HAL_OK) {
            ep.xfer_buff += length;
            ep.xfer_count += length;
        }
    }

    usb_os_spinunlock_safe(pcd->lock);
}

// The SDK can disable the FIFO refill interrupt while some transfer bytes are
// still in RAM. If so, the transfer stops progressing. Once the FIFO has room,
// enable the interrupt again so the SDK can copy the remaining bytes into it.
// xfer_count only counts bytes copied to the FIFO (the host may not have read
// them yet). Wait for the real completion callback before releasing the buffer.
static void service_bulk_in_fifo()
{
    if (configured_device == nullptr || configured_device->pcd == nullptr ||
        bulk_in_state != BulkInState::DATA_IN_FLIGHT) {
        return;
    }

    auto *pcd = static_cast<usbd_pcd_t *>(configured_device->pcd);
    if (pcd->config.dma_enable != 0) {
        return;
    }

    const uint8_t number = bulk_in_ep & 0x7F;
    // Avoid taking the SDK lock when a refill already appears enabled or
    // unnecessary. These first reads can become outdated - check again under
    // the lock before changing registers. A later worker pass retries any
    // recovery we missed by returning early.
    if ((USB_GLOBAL->GINTMSK & USB_OTG_GINTMSK_NPTXFEM) != 0 ||
        (USB_GLOBAL->GINTSTS & USB_OTG_GINTSTS_NPTXFE) == 0) {
        return;
    }
    usb_os_spinlock_safe(pcd->lock);

    const usbd_pcd_ep_t &ep = pcd->in_ep[number];
    const uint32_t control = USB_INEP(number)->DIEPCTL;

    if (ep.type == USB_CH_EP_TYPE_BULK && ep.tx_fifo_num == 0 &&
        ep.max_packet_len != 0 && ep.xfer_buff != nullptr &&
        ep.xfer_count < ep.xfer_len &&
        (control & (USB_OTG_DIEPCTL_EPENA | USB_OTG_DIEPCTL_STALL)) == USB_OTG_DIEPCTL_EPENA &&
        (USB_GLOBAL->GINTSTS & USB_OTG_GINTSTS_NPTXFE) != 0 &&
        (USB_GLOBAL->GINTMSK & USB_OTG_GINTMSK_NPTXFEM) == 0) {
        uint32_t next_packet = ep.xfer_len - ep.xfer_count;
        if (next_packet > ep.max_packet_len) {
            next_packet = ep.max_packet_len;
        }

        const uint32_t words = (next_packet + 3U) / 4U;
        const uint32_t status = USB_GLOBAL->GNPTXSTS;

        if ((status & USB_OTG_GNPTXSTS_NPTQXSAV) != 0 &&
            (status & USB_OTG_GNPTXSTS_NPTXFSAV) > words) {
            // Restore the SDK refill interrupt when unwritten bulk data remains.
            USB_GLOBAL->GINTMSK |= USB_OTG_GINTMSK_NPTXFEM;
            ++usb_transfer_statistics.fifo_rearms;
        }
    }

    usb_os_spinunlock_safe(pcd->lock);
}

static void usb_service_task(void *)
{
    for (;;) {
        ethernet::service_link_state();

        {
            UsbStateLock lock;

            const bool link_up = update_data_path();

            if (configured_device != nullptr &&
                opened_endpoint_count == endpoint_count &&
                configured_device->dev_state == USBD_STATE_CONFIGURED) {
                arm_bulk_out_receive();
                service_bulk_in();
                service_interrupt_in(link_up);
                service_interrupt_fifo();
                service_bulk_in_fifo();
            }
        }

        usb_service_passes++;

        ulTaskNotifyTake(pdTRUE, 1);
    }
}

uint32_t usb_network_service_pass_count()
{
    return usb_service_passes;
}

UsbTransferStatistics usb_network_take_transfer_statistics()
{
    taskENTER_CRITICAL();

    const TickType_t now = xTaskGetTickCount();
    UsbTransferStatistics snapshot = usb_transfer_statistics;
    snapshot.window_ticks = now - usb_statistics_started_tick;
    usb_transfer_statistics = {};
    usb_statistics_started_tick = now;

    taskEXIT_CRITICAL();

    return snapshot;
}

#include "src/diagnostics/usb_network_diagnostics_impl.h"

bool usb_network_begin()
{
    static bool started = false;
    static usbd_class_driver_t driver = {};
    static usbd_config_t configuration = {};

    if (started) {
        return true;
    }

    usb_state_mutex = xSemaphoreCreateRecursiveMutex();
    if (!usb_state_mutex) {
        goto fail;
    }

    driver.get_descriptor = get_descriptor;
    driver.set_config = set_configuration;
    driver.clear_config = clear_configuration;
    driver.setup = handle_setup;
    driver.ep0_data_in = ep0_data_in;
    driver.ep0_data_out = ep0_data_out;
    driver.ep_data_out = data_out;
    driver.ep_data_in = data_in;

    configuration.max_ep_num = 4;
    configuration.rx_fifo_size = USBD_MAX_RX_FIFO_SIZE;
    configuration.nptx_fifo_size = USBD_MAX_NPTX_FIFO_SIZE;
    configuration.ptx_fifo_size = USBD_MAX_PTX_FIFO_SIZE;
    configuration.intr_use_ptx_fifo = 0;
    configuration.speed = USBD_SPEED_HIGH;
    configuration.dma_enable = 0;
    configuration.self_powered = 0;
    configuration.isr_priority = usb_isr_priority;

    if (usbd_init(&configuration) != HAL_OK) {
        goto fail_and_delete_mutex;
    }

    if (usbd_register_class(&driver) != HAL_OK) {
        goto fail_and_deinit_usb;
    }

    if (xTaskCreate(usb_service_task, "usb-net", 512, nullptr, usb_worker_priority, &usb_service_task_handle) != pdPASS) {
        goto fail_and_deinit_usb;
    }

    started = true;
    return true;

fail_and_deinit_usb:
    usbd_deinit();

fail_and_delete_mutex:
    vSemaphoreDelete(usb_state_mutex);
    usb_state_mutex = nullptr;

fail:
    return false;
}

void usb_network_notify_transmit()
{
    if (usb_state_mutex == nullptr) {
        return;
    }

    bool retry_in_worker = false;
    {
        UsbStateLock lock(0);
        if (!lock.is_locked()) {
            retry_in_worker = true;
        } else if (bulk_in_state != BulkInState::DATA_IN_FLIGHT &&
                   bulk_in_state != BulkInState::ZLP_IN_FLIGHT) {
            const bool submitted = service_bulk_in();
            retry_in_worker = !submitted &&
                (bulk_in_state == BulkInState::DATA_READY ||
                 bulk_in_state == BulkInState::ZLP_READY);
        }
    }

    if (retry_in_worker && usb_service_task_handle != nullptr) {
        // Couldn't submit in this context, try again in usb_service_task
        xTaskNotifyGive(usb_service_task_handle);
    }
}
