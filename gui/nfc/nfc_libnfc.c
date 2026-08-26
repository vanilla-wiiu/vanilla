#include "nfc_libnfc.h"
#include "gamepad/nfc.h"
#include "ntag_util.h"
#include "vanilla.h"

#include <nfc/nfc.h>
#include <string.h>
#include <time.h>

#define NTAG_UID_SIZE 7
#define NTAG_VERSION_SIZE 8

static nfc_context *context = NULL;
static nfc_device *device = NULL;
static nfc_target current_target;
static volatile int is_aborted = 0;
static uint8_t cmd_buf[0x3A0];
static uint8_t rx_buf[0x3A0];

static int nfc_libnfc_init(void)
{
    nfc_init(&context);
    if (!context) {
        return VANILLA_ERR_GENERIC;
    }

    // TODO allow device selection?
    device = nfc_open(context, NULL);
    if (!device) {
        nfc_exit(context);
        return VANILLA_ERR_GENERIC;
    }

    if (nfc_initiator_init(device) < 0) {
        nfc_perror(device, "nfc_initiator_init");
        nfc_close(device);
        nfc_exit(context);
        return VANILLA_ERR_GENERIC;
    }

    // Let the device only try once per select to find a tag
    if (nfc_device_set_property_bool(device, NP_INFINITE_SELECT, false) < 0) {
        nfc_perror(device, "nfc_device_set_property_bool");
        nfc_close(device);
        nfc_exit(context);
        return VANILLA_ERR_GENERIC;
    }

    // Easy framing causes errors with NTAG communication
    if (nfc_device_set_property_bool(device, NP_EASY_FRAMING, false) < 0) {
        nfc_perror(device, "nfc_device_set_property_bool");
        nfc_close(device);
        nfc_exit(context);
        return VANILLA_ERR_GENERIC;
    }

    return VANILLA_SUCCESS;
}

static int nfc_libnfc_shutdown(void)
{
    is_aborted = 1;

    nfc_close(device);
    nfc_exit(context);

    return VANILLA_SUCCESS;
}

static int nfc_libnfc_discover(uint16_t timeout, uint8_t tech_mask, VanillaNfcDiscoverData *data)
{
    vanilla_log("nfc_discover");

    is_aborted = 0;

    size_t sz_nms = 0;
    nfc_modulation nms[4];

    // ISO14443-A tags, all other tags are currently unimplemented
    if (tech_mask & NFC_TECHNOLOGY_MASK_A) {
        nms[sz_nms].nmt = NMT_ISO14443A;
        nms[sz_nms].nbr = NBR_106;
        sz_nms++;
    }

    struct timespec start, current;
    uint64_t elapsed_ms;
    clock_gettime(CLOCK_MONOTONIC, &start);

    do {
        for (size_t i = 0; i < sz_nms; i++) {
            if (nfc_initiator_select_passive_target(device, nms[i], NULL, 0, &current_target) <= 0) {
                continue;
            }

            switch (current_target.nm.nmt) {
            case NMT_ISO14443A: {
                // TODO not every ISO type A will be a T2T
                data->protocol = NFC_PROTOCOL_T2T;

                data->uid_size = current_target.nti.nai.szUidLen;
                memcpy(data->uid, current_target.nti.nai.abtUid, data->uid_size);

                return VANILLA_SUCCESS;
            }
            default:
                return VANILLA_ERR_GENERIC;
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &current);
        elapsed_ms = (current.tv_sec - start.tv_sec) * 1000 + (current.tv_nsec - start.tv_nsec) / 1000000;
    } while ((timeout == 0 || elapsed_ms < timeout) && !is_aborted);

    vanilla_log("nfc_discover timed out");

    return VANILLA_ERR_TIMEOUT;
}

static void nfc_libnfc_abort_discover(void)
{
    is_aborted = 1;
}

static uint8_t _nfc_t2t_check(const uint8_t uid[NTAG_UID_SIZE], const uint8_t uid_mask[NTAG_UID_SIZE],
                              uint8_t version[NTAG_VERSION_SIZE])
{
    if (current_target.nm.nmt != NMT_ISO14443A) {
        vanilla_log("not t2t %d\n", current_target.nm.nmt);
        return 0xd;
    }

    // Perform UID check of current selected tag
    for (int i = 0; i < NTAG_UID_SIZE; i++) {
        if ((current_target.nti.nai.abtUid[i] & uid_mask[i]) != uid[i]) {
            vanilla_log("uid mismatch");
            return 0xa;
        }
    }

    size_t cmd_size, rx_size;
    ntag_prepare_get_version_cmd(cmd_buf, &cmd_size, &rx_size);
    if (nfc_initiator_transceive_bytes(device, cmd_buf, cmd_size, rx_buf, rx_size, -1) != rx_size) {
        vanilla_log("GET_VERSION fail");
        return 0xd;
    }

    memcpy(version, rx_buf, rx_size);
    return 0;
}

static uint8_t _nfc_t2t_pwd_auth(const uint8_t uid[NTAG_UID_SIZE])
{
    size_t cmd_size, rx_size;

    // Read CC
    ntag_prepare_read_cmd(3, cmd_buf, &cmd_size, &rx_size);
    if (nfc_initiator_transceive_bytes(device, cmd_buf, cmd_size, rx_buf, rx_size, -1) != rx_size) {
        vanilla_log("READ CC fail");
        return 0xd;
    }

    // Verify CC
    if (memcmp(rx_buf, "\xF1\x10\xFF\xEE", 4) != 0) {
        vanilla_log("CC check fail");
        return 0xd;
    }

    // Do PWD auth
    uint8_t pwd[4];
    pwd[0] = uid[1] ^ uid[3] ^ 0xaa;
    pwd[1] = uid[2] ^ uid[4] ^ 0x55;
    pwd[2] = uid[3] ^ uid[5] ^ 0xaa;
    pwd[3] = uid[4] ^ uid[6] ^ 0x55;
    ntag_prepare_pwd_auth_cmd(pwd, cmd_buf, &cmd_size, &rx_size);
    if (nfc_initiator_transceive_bytes(device, cmd_buf, cmd_size, rx_buf, rx_size, -1) != rx_size) {
        vanilla_log("PWD_AUTH fail");
        return 0xd;
    }

    // Verify PACK
    if (memcmp(rx_buf, "\x80\x80", 2) != 0) {
        vanilla_log("PACK check fail");
        return 0xd;
    }

    return 0;
}

static uint8_t nfc_libnfc_read_t2t(const VanillaNfcReadT2TParams *params, VanillaNfcReadT2TData *data)
{
    uint8_t res;

    vanilla_log("nfc_read_t2t");

    // Start by doing common common pre check and get version
    uint8_t version[NTAG_VERSION_SIZE];
    res = _nfc_t2t_check(params->uid, params->uid_mask, version);
    if (res != 0) {
        return res;
    }

    // Read silicon vendor signature
    size_t cmd_size, rx_size;
    ntag_prepare_read_sig_cmd(cmd_buf, &cmd_size, &rx_size);
    if (nfc_initiator_transceive_bytes(device, cmd_buf, cmd_size, rx_buf, rx_size, -1) != rx_size) {
        vanilla_log("READ_SIG fail");
        return 0xd;
    }
    memcpy(data->signature, rx_buf, rx_size);

    // Do pwd authentication if requested
    if (params->perform_pwd_auth) {
        res = _nfc_t2t_pwd_auth(current_target.nti.nai.abtUid);
        if (res != 0) {
            return res;
        }
    }

    // Read data ranges
    uint8_t *p_data = data->data;
    for (int i = 0; i < params->num_ranges; i++) {
        ntag_prepare_fast_read_cmd(params->ranges[i].start, params->ranges[i].end, cmd_buf, &cmd_size, &rx_size);
        if (nfc_initiator_transceive_bytes(device, cmd_buf, cmd_size, rx_buf, rx_size, -1) != rx_size) {
            vanilla_log("FAST_READ fail");
            return 0x3;
        }

        memcpy(p_data, rx_buf, rx_size);
        p_data += rx_size;
    }

    return 0;
}

static bool _nfc_ntag_write(uint8_t addr, const uint8_t data[4])
{
    // Easy framing seems to be required for basic write cmds
    if (nfc_device_set_property_bool(device, NP_EASY_FRAMING, true) < 0) {
        nfc_perror(device, "nfc_device_set_property_bool");
        return false;
    }

    size_t cmd_size, rx_size;
    ntag_prepare_write_cmd(addr, data, cmd_buf, &cmd_size, &rx_size);
    int res = nfc_initiator_transceive_bytes(device, cmd_buf, cmd_size, rx_buf, rx_size, -1);

    // Turn easy framing off again
    if (nfc_device_set_property_bool(device, NP_EASY_FRAMING, false) < 0) {
        nfc_perror(device, "nfc_device_set_property_bool");
        return false;
    }

    if (res != 0) { // NOTE: easy framing result handles NAK and ACK?
        vanilla_log("WRITE fail %d", res);
        return false;
    }

    return true;
}

static uint8_t nfc_libnfc_write_t2t(const VanillaNfcWriteT2TParams *params)
{
    uint8_t res;

    vanilla_log("nfc_write_t2t");

    // Start by doing common common pre check and get version
    uint8_t version[NTAG_VERSION_SIZE];
    res = _nfc_t2t_check(params->uid, params->uid_mask, version);
    if (res != 0) {
        return res;
    }

    // Make sure version matches
    if (memcmp(version, params->expected_version, sizeof(version)) != 0) {
        return 0xd;
    }

    // Do pwd authentication if requested
    if (params->perform_pwd_auth) {
        res = _nfc_t2t_pwd_auth(current_target.nti.nai.abtUid);
        if (res != 0) {
            return res;
        }
    }

    // Deactivate the tag first if requested
    if (params->perform_activation) {
        if (!_nfc_ntag_write(params->activation_address, params->deactivation_data)) {
            vanilla_log("deactivation failed");
            return 0x4;
        }
    }

    for (int i = 0; i < params->num_ranges; i++) {
        const uint8_t address = params->ranges[i].address;
        const uint8_t num_pages = (params->ranges[i].size + 3) / NTAG_PAGE_SIZE;

        // Write data
        for (int j = 0; j < num_pages; j++) {
            if (!_nfc_ntag_write(address + j, params->ranges[i].data + j * NTAG_PAGE_SIZE)) {
                vanilla_log("write failed");
                return 0x4;
            }
        }

        // Read back data and compare
        size_t cmd_size, rx_size;
        ntag_prepare_fast_read_cmd(address, address + num_pages - 1, cmd_buf, &cmd_size, &rx_size);
        if (nfc_initiator_transceive_bytes(device, cmd_buf, cmd_size, rx_buf, rx_size, -1) != rx_size) {
            vanilla_log("FAST_READ fail");
            return 0x3;
        }

        if (memcmp(rx_buf, params->ranges[i].data, params->ranges[i].size) != 0) {
            return 0x5;
        }
    }

    // Activate the tag if requested
    if (params->perform_activation) {
        if (!_nfc_ntag_write(params->activation_address, params->activation_data)) {
            vanilla_log("activation failed");
            return 0x17;
        }

        // Read back data and compare
        size_t cmd_size, rx_size;
        ntag_prepare_fast_read_cmd(params->activation_address, params->activation_address, cmd_buf, &cmd_size,
                                   &rx_size);
        if (nfc_initiator_transceive_bytes(device, cmd_buf, cmd_size, rx_buf, rx_size, -1) != rx_size) {
            vanilla_log("FAST_READ fail");
            return 0x17;
        }

        if (memcmp(rx_buf, params->activation_data, sizeof(params->activation_data)) != 0) {
            vanilla_log("activation mismatch");
            return 0x17;
        }
    }

    return 0;
}

VanillaNfcBackend nfc_libnfc_backend = {
    .init = nfc_libnfc_init,
    .shutdown = nfc_libnfc_shutdown,
    .discover = nfc_libnfc_discover,
    .abort_discover = nfc_libnfc_abort_discover,

    .read_t2t = nfc_libnfc_read_t2t,
    .write_t2t = nfc_libnfc_write_t2t,
};
