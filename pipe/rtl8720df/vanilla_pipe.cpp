#include "vanilla_pipe.h"

extern "C" {
#include "FreeRTOS.h"
#include "queue.h"
#include "lwip/def.h"
#include "lwip/ip.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/tcpip.h"
#include "lwip/timeouts.h"
#include "lwip/udp.h"
#include "task.h"

#include "wifi/rtw_wpa_supplicant/src/wps/wps_defs.h"
#include "wifi/rtw_wpa_supplicant/wpa_supplicant/wifi_wps_config.h"

extern char wps_profile_ssid[33];
extern char wps_profile_password[65];

extern struct netif xnetif[];
}

#include <string.h>
#include <wifi_conf.h>
#include <wifi_drv.h>
#include <wl_types.h>

#include "src/diagnostics/debug_log.h"
#include "ethernet_definitions.h"
#include "udp_relay.h"
#include "vanilla_pipe_def.h"

namespace vanilla_pipe {

static struct udp_pcb *server_pcb = nullptr;
static struct netif *server_interface = nullptr;

struct WorkItem {
    vanilla_pipe_command_t command;
    ip_addr_t destination;
    uint32_t generation;
};

enum class WorkResultKind : uint8_t {
    OPERATION_COMPLETE,
    LINK_STATE
};

struct WorkResult {
    ip_addr_t destination;
    vanilla_pipe_command_t reply;
    uint16_t length;
    uint32_t generation;
    WorkResultKind kind;
};

static QueueHandle_t completion_queue = nullptr;
static QueueHandle_t work_queue = nullptr;

// Accessed only by Arduino worker context
static bool completion_needs_posting = false;

// Used to coordinate with asynchronous Wi-Fi connection - if a new connection
// starts or existing one is canceled, we increment "generation" so that anything
// for an earlier connection that finished late doesn't mistakenly get re-sent
// over a new connection
static uint32_t operation_generation = 0;
static bool disconnect_requested = false;

static bool monitor_enabled = false;
static bool monitor_reported_up = false;
static uint32_t monitor_generation = 0;

enum class SessionState {
    IDLE,
    SYNCING,
    SYNCED,
    CONNECTING,
    CONNECTED,
    DISCONNECTED
};

// Accessed only from lwIP
static SessionState session_state = SessionState::IDLE;
static ip_addr_t session_destination = {};
static vanilla_pipe_command_t session_command = {};
static vanilla_pipe_command_t session_reply = {};
static uint16_t session_reply_length;

static bool operation_running = false;
static uint32_t running_generation = 0;

static constexpr uint32_t heartbeat_interval_ms = 500;
static constexpr uint32_t link_check_interval_ms = 250;

static uint32_t current_generation()
{
    taskENTER_CRITICAL();
    const uint32_t generation = operation_generation;
    taskEXIT_CRITICAL();

    return generation;
}

// Call only from lwIP
static uint32_t advance_generation()
{
    taskENTER_CRITICAL();
    const uint32_t generation = ++operation_generation;
    taskEXIT_CRITICAL();

    return generation;
}

// Call only from lwIP
static void cancel_operation()
{
    taskENTER_CRITICAL();
    operation_generation++;
    disconnect_requested = true;
    taskEXIT_CRITICAL();
}

// Call only from lwIP
static void update_session_services(uint32_t generation)
{
    taskENTER_CRITICAL();

    monitor_enabled = session_state == SessionState::CONNECTED || session_state == SessionState::DISCONNECTED;

    monitor_reported_up = session_state == SessionState::CONNECTED;

    monitor_generation = generation;

    taskEXIT_CRITICAL();

    udp_relay::set_session(session_destination, session_state == SessionState::CONNECTED);
}

static void clear_session()
{
    session_state = SessionState::IDLE;
    session_destination = {};
    session_command = {};
    session_reply = {};
    session_reply_length = 0;

    update_session_services(0);
}

static void cancel_session()
{
    cancel_operation();
    clear_session();
}

void link_down()
{
    if (session_state != SessionState::IDLE || operation_running) {
        cancel_session();
    }
}

static bool send_reply(const ip_addr_t &destination, const void *data, uint16_t length)
{
    if (!server_pcb || !server_interface || !netif_is_up(server_interface)
        || !netif_is_link_up(server_interface) || !data || !length
        || length > sizeof(vanilla_pipe_command_t)) {
        return false;
    }

    struct pbuf *packet = pbuf_alloc(PBUF_TRANSPORT, length, PBUF_RAM);
    if (!packet) {
        return false;
    }

    err_t result = pbuf_take(packet, data, length);
    if (result == ERR_OK) {
        result = udp_sendto_if(server_pcb, packet, &destination, VANILLA_PIPE_CMD_CLIENT_PORT, server_interface);
    }

    pbuf_free(packet);

    return result == ERR_OK;
}

static bool send_code(const ip_addr_t &destination, uint8_t code)
{
    return send_reply(destination, &code, sizeof(code));
}

// Runs on lwIP thread
static void heartbeat_tick(void *context)
{
    (void)context;

    if (session_state != SessionState::IDLE) {
        if (!send_code (session_destination, VANILLA_PIPE_CC_PING)) {
            NETWORK_DEBUG("Pipe heartbeat send failed\n");
        }
    }

    sys_timeout(heartbeat_interval_ms, heartbeat_tick, nullptr);
}

static void deliver_completion(void *context)
{
    (void)context;

    WorkResult result = {};

    if (xQueueReceive(completion_queue, &result, 0) != pdPASS) {
        return;
    }

    if (result.kind == WorkResultKind::LINK_STATE) {
        if (result.generation != current_generation() ||
            (session_state != SessionState::CONNECTED &&
            session_state != SessionState::DISCONNECTED)) {
            return;
        }

        const bool up = result.reply.control_code == VANILLA_PIPE_CC_CONNECTED;

        session_state = up
            ? SessionState::CONNECTED
            : SessionState::DISCONNECTED;

        session_reply = result.reply;
        session_reply_length = result.length;

        update_session_services(result.generation);

        if (!send_reply(session_destination, &session_reply, session_reply_length)) {
            NETWORK_DEBUG("Pipe link notification failed\n");
        }

        return;
    }

    if (!operation_running || result.generation != running_generation) {
        return;
    }

    operation_running = false;

    if (result.generation != current_generation() ||
        (session_state != SessionState::CONNECTING && session_state != SessionState::SYNCING)) {
        return;
    }
    
    if (session_state == SessionState::SYNCING && result.reply.control_code == VANILLA_PIPE_CC_SYNC_SUCCESS) {
        session_state = SessionState::SYNCED;
    } else if (session_state == SessionState::CONNECTING && result.reply.control_code == VANILLA_PIPE_CC_CONNECTED) {
        session_state = SessionState::CONNECTED;
    } else {
        clear_session();
    }

    if (session_state != SessionState::IDLE) {
        session_reply = result.reply;
        session_reply_length = result.length;
    }

    update_session_services(result.generation);

    if (!send_reply(result.destination, &result.reply, result.length)) {
        NETWORK_DEBUG("Pipe completion reply failed\n");
    }
}

// Call only from Arduino worker context
static bool queue_completion(const WorkResult &result)
{
    if (xQueueSend(completion_queue, &result, 0) != pdPASS) {
        return false;
    }

    completion_needs_posting = true;
    return true;
}

static bool read_command(const struct pbuf *packet, vanilla_pipe_command_t &command)
{
    if (!packet || packet->tot_len < 1 || packet->tot_len > sizeof(command)) {
        return false;
    }

    uint8_t code = 0;

    if (pbuf_copy_partial(packet, &code, sizeof(code), 0) != sizeof(code)) { 
        return false;
    }

    uint16_t expected_length = 0;

    switch (code) {
    case VANILLA_PIPE_CC_SYNC:
        expected_length = sizeof(command.control_code) + sizeof(command.sync);
        break;
    case VANILLA_PIPE_CC_CONNECT:
        expected_length = sizeof(command.control_code) + sizeof(command.connection);
        break;
    case VANILLA_PIPE_CC_UNBIND:
        expected_length = sizeof(command.control_code);
        break;
    default:
        return false;
    }

    if (packet->tot_len != expected_length) {
        return false;
    }

    vanilla_pipe_command_t candidate = {};

    if (pbuf_copy_partial(packet, &candidate, expected_length, 0) != expected_length) {
        return false;
    }

    if (code == VANILLA_PIPE_CC_SYNC && lwip_ntohs(candidate.sync.code) > 9999U) {
        return false;
    }

    command = candidate;
    return true;
}

static bool same_session_request(const vanilla_pipe_command_t &command)
{
    if (command.control_code != session_command.control_code) {
        return false;
    }

    switch (command.control_code) {
    case VANILLA_PIPE_CC_SYNC:
        return command.sync.code == session_command.sync.code;
    case VANILLA_PIPE_CC_CONNECT:
        return !memcmp(&command.connection, &session_command.connection, sizeof(command.connection));
    default:
        return false;
    }
}

static void handle_command(const vanilla_pipe_command_t &command, const ip_addr_t &destination)
{
    if (command.control_code == VANILLA_PIPE_CC_UNBIND) {
        if (session_state != SessionState::IDLE && ip_addr_cmp(&destination, &session_destination)) {
            cancel_session();
        }
        return;
    }

    if (command.control_code != VANILLA_PIPE_CC_CONNECT && command.control_code != VANILLA_PIPE_CC_SYNC) {
        return;
    }

    if (session_state != SessionState::IDLE) {
        const bool same_peer = ip_addr_cmp(&destination, &session_destination);

        if (same_peer && same_session_request(command)) {
            send_code(destination, VANILLA_PIPE_CC_BIND_ACK);

            if (session_reply_length != 0) {
                send_reply(destination, &session_reply, session_reply_length);
            }

            return;
        }

        if (session_state != SessionState::SYNCED || !same_peer) {
            send_code(destination, VANILLA_PIPE_CC_BUSY);
            return;
        }
    }

    if (operation_running) {
        send_code(destination, VANILLA_PIPE_CC_BUSY);
        return;
    }

    WorkItem work = {};
    work.command = command;
    work.destination = destination;
    work.generation = advance_generation();

    if (xQueueSend(work_queue, &work, 0) != pdPASS) {
        send_code(destination, VANILLA_PIPE_CC_BUSY);
        return;
    }

    session_destination = destination;
    session_command = command;
    session_reply = {};
    session_reply_length = 0;

    session_state = command.control_code == VANILLA_PIPE_CC_SYNC
        ? SessionState::SYNCING
        : SessionState::CONNECTING;

    update_session_services(work.generation);

    operation_running = true;
    running_generation = work.generation;

    send_code(destination, VANILLA_PIPE_CC_BIND_ACK);
}

static void receive_packet(void *context, struct udp_pcb *pcb, struct pbuf *packet, const ip_addr_t *address, u16_t port)
{
    (void)pcb;

    if (!packet) {
        return;
    }

    auto *interface = static_cast<struct netif *>(context);

    if (ip_current_input_netif() == interface &&
        netif_is_up(interface) &&
        netif_is_link_up(interface) &&
        port == VANILLA_PIPE_CMD_CLIENT_PORT) {

        vanilla_pipe_command_t command = {};

        if (read_command(packet, command)) {
            NETWORK_DEBUG("Pipe command=%x\n", (unsigned) command.control_code);

            const ip_addr_t destination = *address;

            handle_command(command, destination);
        }
    }

    pbuf_free(packet);
}

static void encode_hex(char *destination, const uint8_t *source, size_t length)
{
    static const char digits[] = "0123456789abcdef";

    for (size_t i = 0; i < length; i++) {
        destination[2 * i] = digits[source[i] >> 4];
        destination[2 * i + 1] = digits[source[i] & 0x0F];
    }

    destination[2 * length] = '\0';
}

// Code is in host byte order
static bool make_sync_pin(uint16_t code, char (&pin)[9])
{
    if (code > 9999U) {
        return false;
    }

    char candidate[sizeof(pin)] = "00005678";

    for (size_t i = 4; i > 0; i--) {
        candidate[i - 1] = '0' + code % 10;
        code /= 10;
    }

    memcpy(pin, candidate, sizeof(candidate));

    return true;
}

static int hex_value(char value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }

    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }

    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }

    return -1;
}

static bool decode_hex(const char *source, uint8_t *destination, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        const int high = hex_value(source[2 * i]);
        const int low = hex_value(source[2 * i + 1]);

        if (high < 0 || low < 0) {
            return false;
        }

        destination[i] = ((high << 4) | low);
    }

    return true;
}

static bool read_pairing_profile(const char (&ssid)[33], const char (&password)[65], vanilla_connection_t &connection)
{
    constexpr size_t prefix_length = sizeof("WiiU") - 1;
    constexpr size_t ssid_length = prefix_length + 2 * sizeof(connection.bssid.bssid);
    constexpr size_t password_length = 2 * sizeof(connection.psk.psk);

    if (ssid[ssid_length] != '\0' || password[password_length] != '\0' || memcmp(ssid, "WiiU", prefix_length)) {
        return false;
    }

    vanilla_connection_t candidate = {};

    if (!decode_hex(ssid + prefix_length, candidate.bssid.bssid, sizeof(candidate.bssid.bssid))
        || !decode_hex(password, candidate.psk.psk, sizeof(candidate.psk.psk))) {
        return false;
    }

    connection = candidate;
    return true;
}

// Call only from Arduino worker context
static bool connect_to_console(const vanilla_connection_t &connection)
{
    char ssid[sizeof("WiiU") + 2 * sizeof(connection.bssid.bssid)] = "WiiU";
    char psk[2 * sizeof(connection.psk.psk) + 1] = {};

    encode_hex(ssid + sizeof("WiiU") - 1, connection.bssid.bssid, sizeof(connection.bssid.bssid));
    encode_hex(psk, connection.psk.psk, sizeof(connection.psk.psk));

    const bool connected = WiFiDrv::wifiSetPassphrase(ssid, sizeof(ssid)-1, psk, sizeof(psk)-1) == WL_SUCCESS;

    memset(psk, 0, sizeof(psk));

    return connected;
}

static bool is_console_pairing_ssid(const uint8_t *ssid, size_t length)
{
    constexpr char prefix[] = "WiiU";
    constexpr char suffix[] = "_STA1";

    constexpr size_t prefix_length = sizeof(prefix) - 1;
    constexpr size_t suffix_length = sizeof(suffix) - 1;

    if (!ssid || length > 32 || length <= prefix_length + suffix_length || memchr(ssid, 0, length) != nullptr) {
        return false;
    }

    return memcmp(ssid, prefix, prefix_length) == 0 && memcmp(ssid + length - suffix_length, suffix, suffix_length) == 0;
}

struct PairingTarget {
    char ssid[33];
    int32_t signal_strength;
    bool found;
};

static bool read_pairing_scan(const uint8_t *data, size_t length, PairingTarget &target)
{
    // Each scan entry contains length, BSSID, RSSI, security, WPS ID, channel,
    // then SSID bytes. Check length before reading fields so malformed input
    // doesn't overflow. The four-byte RSSI follows seven bytes, so its address
    // may not be divisible by four as C++ int32_t requires here.
    // memcpy copies its bytes without assuming that address alignment.
    constexpr size_t rssi_offset = 1 + ETH_ALEN;
    constexpr size_t header_length = rssi_offset + sizeof(int32_t) + 3;

    if (!data || !length) {
        return false;
    }

    PairingTarget candidate = {};
    size_t offset = 0;

    while (offset < length) {
        const size_t record_length = data[offset];
        
        if (!record_length) {
            break;
        }

        if (record_length < header_length || record_length > length - offset) {
            return false;
        }

        const uint8_t *record = data + offset;
        const size_t ssid_length = record_length - header_length;

        if (ssid_length > sizeof(candidate.ssid) - 1) {
            return false;
        }

        if (record[header_length - 1] != 0 && is_console_pairing_ssid(record + header_length, ssid_length)) {
            int32_t signal_strength;
            memcpy(&signal_strength, record + rssi_offset, sizeof(signal_strength));
            if (!candidate.found || signal_strength > candidate.signal_strength) {
                candidate = {};
                memcpy(candidate.ssid, record + header_length, ssid_length);
                candidate.signal_strength = signal_strength;
                candidate.found = true;
            }
        }

        offset += record_length;
    }

    target = candidate;
    return true;
}

static int receive_pairing_scan(char *buffer, int length, char *ssid, void *context)
{
    (void)ssid;

    if (!context || length <= 0) {
        return RTW_ERROR;
    }

    auto *target = static_cast<PairingTarget *>(context);

    const bool valid = read_pairing_scan(reinterpret_cast<const uint8_t *>(buffer), length, *target);

    return valid ? RTW_SUCCESS : RTW_ERROR;
}

// Call only from Arduino worker context
static bool find_pairing_target(PairingTarget &target)
{
    // The SDK calls receive_pairing_scan() before this scan call returns, then
    // frees the scan buffer. The callback must copy the selected SSID into
    // target - keeping a pointer to the SDK's SSID would leave it pointing at
    // freed memory after the scan returns.
    constexpr int scan_buffer_size = 4096;
    char ssid_filter[] = "";
    PairingTarget candidate = {};

    const int status = wifi_scan_networks_with_ssid(
        receive_pairing_scan,
        &candidate,
        scan_buffer_size,
        ssid_filter,
        0
    );

    if (status != RTW_SUCCESS || !candidate.found) {
        return false;
    }

    target = candidate;
    return true;
}

// Call only from Arduino worker context
static bool wait_for_pairing_target(PairingTarget &target, uint32_t generation)
{
    const TickType_t timeout = pdMS_TO_TICKS(120000U);
    const TickType_t retry_delay = pdMS_TO_TICKS(500U);
    const TickType_t started = xTaskGetTickCount();

    while (1) {
        if (generation != current_generation() || xTaskGetTickCount() - started >= timeout) {
            return false;
        }

        PairingTarget candidate = {};
        const bool found = find_pairing_target(candidate);

        if (generation != current_generation()) {
            return false;
        }

        const TickType_t elapsed = xTaskGetTickCount() - started;

        if (elapsed >= timeout) {
            return false;
        }

        if (found) {
            target = candidate;
            return true;
        }

        const TickType_t remaining = timeout - elapsed;

        vTaskDelay(remaining < retry_delay ? remaining : retry_delay);
    }
}

// Call only from Arduino worker context
// `code` is in host byte order
static bool sync_with_console(uint16_t code, vanilla_connection_t &connection, uint32_t generation)
{
    char pin[9] = {};

    if (!make_sync_pin(code, pin)) {
        return false;
    }

    PairingTarget target = {};

    // wps_start can technically scan for Wi-Fi networks itself, however I found that
    // it did not see the Wii U (possibly due to different WPS IEs). However, the chip does
    // find it in regular scans, and if we pass the SSID directly to wps_start. So we
    // do our own scan first, and then pass it along to make pairing work properly.

    if (generation != current_generation()
        || !wait_for_pairing_target(target, generation)
        || generation != current_generation()) {
        memset(pin, 0, sizeof(pin));
        return false;
    }

    memset(wps_profile_ssid, 0, sizeof(wps_profile_ssid));
    memset(wps_profile_password, 0, sizeof(wps_profile_password));

    const int status = wps_start(WPS_CONFIG_KEYPAD, pin, 0, target.ssid);

    memset(pin, 0, sizeof(pin));

    if (status != 0 || generation != current_generation()) {
        return false;
    }

    return read_pairing_profile(wps_profile_ssid, wps_profile_password, connection);
}

static WorkResult execute_work(const WorkItem &work)
{
    WorkResult result = {};
    result.destination = work.destination;
    result.generation = work.generation;

    switch (work.command.control_code) {
    case VANILLA_PIPE_CC_SYNC:
        if (sync_with_console(lwip_ntohs(work.command.sync.code), result.reply.connection, work.generation)) {
            result.reply.control_code = VANILLA_PIPE_CC_SYNC_SUCCESS;
            result.length = sizeof(result.reply.control_code) + sizeof(result.reply.connection);
        } else {
            result.reply.control_code = VANILLA_PIPE_CC_STATUS;
            result.reply.status.status = lwip_htonl(VANILLA_ERR_GENERIC);
            result.length = sizeof(result.reply.control_code) + sizeof(result.reply.status);
        }
        break;
    case VANILLA_PIPE_CC_CONNECT:
        result.reply.control_code = connect_to_console(work.command.connection) ? VANILLA_PIPE_CC_CONNECTED : VANILLA_PIPE_CC_DISCONNECTED;
        result.length = sizeof(result.reply.control_code);
        break;
    default:
        result.reply.control_code = VANILLA_PIPE_CC_STATUS;
        result.reply.status.status = lwip_htonl(VANILLA_ERR_UNKNOWN_COMMAND);
        result.length = sizeof(result.reply.control_code) + sizeof(result.reply.status);
        break;
    }

    return result;
}

// Call from service() only when the completion queue has space.
static bool check_wifi_link()
{
    static TickType_t last_check = 0;
    const TickType_t now = xTaskGetTickCount();

    // Don't take a shared-state snapshot every loop, do it on an interval
    if (static_cast<TickType_t>(now - last_check) < pdMS_TO_TICKS(link_check_interval_ms)) {
        return false;
    }

    bool enabled;
    bool reported_up;
    uint32_t generation;

    taskENTER_CRITICAL();
    enabled = monitor_enabled;
    reported_up = monitor_reported_up;
    generation = monitor_generation;
    taskEXIT_CRITICAL();

    if (!enabled) {
        return false;
    }

    last_check = now;

    // Use wifi_is_connected_to_ap() instead of  WiFi.status()/WiFiDrv::getConnectionStatus()
    // to check connected state since those functions call wifiDriverInit() which may stop
    // or restart Wi-Fi if the driver's mode differs from Arduino's mode
    bool up = wifi_is_connected_to_ap() == RTW_SUCCESS;

    if (up) {
        IPAddress address;
        WiFiDrv::getIpAddress(address);
        up = address != 0;
    }

    if (up == reported_up) {
        return false;
    }

    WorkResult result = {};
    result.kind = WorkResultKind::LINK_STATE;
    result.generation = generation;

    result.reply.control_code = up ? VANILLA_PIPE_CC_CONNECTED : VANILLA_PIPE_CC_DISCONNECTED;

    result.length = sizeof(result.reply.control_code);

    const bool queued = queue_completion(result);
    configASSERT(queued);
    (void)queued;

    return true;
}

void service()
{
    if (!work_queue || !completion_queue) {
        return;
    }

    bool disconnect;
    taskENTER_CRITICAL();
    disconnect = disconnect_requested;
    disconnect_requested = false;
    taskEXIT_CRITICAL();

    if (disconnect) {
        WiFiDrv::disconnect();
        return;
    }

    if (completion_needs_posting) {
        if (tcpip_callback_with_block(deliver_completion, nullptr, 0) == ERR_OK) {
            completion_needs_posting = false;
        }
        return;
    }

    if (!uxQueueSpacesAvailable(completion_queue)) {
        return;
    }

    if (check_wifi_link()) {
        return;
    }

    WorkItem work = {};

    if (xQueueReceive(work_queue, &work, 0) != pdPASS) {
        return;
    }

    WorkResult result = {};
    result.destination = work.destination;
    result.generation = work.generation;
    if (work.generation == current_generation()) {
        result = execute_work(work);
    }

    const bool queued = queue_completion(result);
    configASSERT(queued);
    (void)queued;
}

bool begin(struct netif *interface)
{
    struct udp_pcb *candidate;
    ip_addr_t local_address;
    QueueHandle_t requests;
    QueueHandle_t completions;

    if (!interface) {
        goto die;
    }

    if (server_pcb) {
        return server_interface == interface;
    }

    candidate = udp_new_ip_type(IPADDR_TYPE_V4);

    if (!candidate) {
        goto die;
    }

    ip_addr_copy_from_ip4(local_address, *netif_ip4_addr(interface));

    if (udp_bind(candidate, &local_address, VANILLA_PIPE_CMD_SERVER_PORT) != ERR_OK) {
        goto die_and_remove_udp;
    }

    requests = xQueueCreate(1, sizeof(WorkItem));
    if (!requests) {
        goto die_and_remove_udp;
    }

    completions = xQueueCreate(1, sizeof(WorkResult));
    if (!completions) {
        goto die_and_delete_requests;
    }

    if (!udp_relay::begin(interface, &xnetif[0])) {
        goto die_and_delete_completions;
    }

    completion_queue = completions;
    work_queue = requests;

    server_pcb = candidate;
    server_interface = interface;

    udp_recv(server_pcb, receive_packet, interface);

    sys_timeout(heartbeat_interval_ms, heartbeat_tick, nullptr);

    return true;

die_and_delete_completions:
    vQueueDelete(completions);

die_and_delete_requests:
    vQueueDelete(requests);

die_and_remove_udp:
    udp_remove(candidate);

die:
    return false;
}

}
