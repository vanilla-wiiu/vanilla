#define _GNU_SOURCE

#include "nfc.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "input.h"
#include "util.h"
#include "vanilla.h"

static pthread_t nfc_thread;
static pthread_mutex_t nfc_thread_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t nfc_thread_cond = PTHREAD_COND_INITIALIZER;

enum {
    NFC_STATE_UNINITIALIZED,
    NFC_STATE_INITIALIZED,
    NFC_STATE_READY,
    NFC_STATE_DISCOVERY,
    NFC_STATE_DISCOVERED,
    NFC_STATE_COMPLETED,
    NFC_STATE_ABORTED,
    NFC_STATE_MULTI_DISCOVERY,
    NFC_STATE_MULTI_DISCOVERED,
    NFC_STATE_ANTENNA_CHECK,
    NFC_STATE_ANTENNA_CHECKED,
};
static volatile int nfc_state = NFC_STATE_UNINITIALIZED;

enum {
    NFC_THREAD_COMMAND_READ,
    NFC_THREAD_COMMAND_WRITE,
    NFC_THREAD_COMMAND_FORMAT,
    NFC_THREAD_COMMAND_SET_READ_ONLY,
    NFC_THREAD_COMMAND_IS_TAG_PRESENT,
    NFC_THREAD_COMMAND_PASS_THROUGH_SEND,
    NFC_THREAD_COMMAND_DETECT,
    NFC_THREAD_COMMAND_DETECT_MULTI,
    NFC_THREAD_COMMAND_PASS_THROUGH_SEND2,
    NFC_THREAD_COMMAND_READ_T2T,
    NFC_THREAD_COMMAND_WRITE_T2T,
    NFC_THREAD_COMMAND_NONE,
    NFC_THREAD_COMMAND_QUIT,
};
static int nfc_thread_command = NFC_THREAD_COMMAND_NONE;

static const VanillaNfcBackend *nfc_backend = NULL;

static uint8_t nfc_result = 0;
static uint16_t nfc_discovery_timeout;
static VanillaNfcDiscoverData nfc_discover_data;
static VanillaNfcReadT2TParams nfc_read_t2t_params;
static VanillaNfcReadT2TData nfc_read_t2t_data;
static VanillaNfcWriteT2TParams nfc_write_t2t_params;

static void nfc_start_thread_command(int command)
{
    nfc_thread_command = command;
    pthread_cond_signal(&nfc_thread_cond);
}

static void nfc_proc_pass_through_send()
{
    // TODO implement actual pass through send

    set_status_flag(STATUS_NFC_MODE, 1);
}

static void nfc_proc_read_t2t()
{
    set_status_flag(STATUS_NFC_MODE, 1);

    nfc_result = nfc_backend->read_t2t(&nfc_read_t2t_params, &nfc_read_t2t_data);

    set_status_flag(STATUS_NFC_MODE, 0);
}

static void nfc_proc_write_t2t()
{
    set_status_flag(STATUS_NFC_MODE, 1);

    nfc_result = nfc_backend->write_t2t(&nfc_write_t2t_params);

    set_status_flag(STATUS_NFC_MODE, 0);
}

static void nfc_process_thread_command(void)
{
    nfc_state = NFC_STATE_DISCOVERY;

    // Start discovery
    int result = nfc_backend->discover(nfc_discovery_timeout, 0xf, &nfc_discover_data);
    if (result != VANILLA_SUCCESS) {
        if (nfc_state == NFC_STATE_ABORTED) {
            nfc_result = 6;
            return;
        }

        // timed out
        if (result == VANILLA_ERR_TIMEOUT) {
            nfc_state = NFC_STATE_COMPLETED;
            nfc_result = 1;
            return;
        }

        nfc_result = 6;
        return;
    }

    nfc_state = NFC_STATE_DISCOVERED;

    if (nfc_thread_command != NFC_THREAD_COMMAND_PASS_THROUGH_SEND &&
        nfc_thread_command != NFC_THREAD_COMMAND_PASS_THROUGH_SEND2) {

        set_status_flag(STATUS_NFC_TAG_FOUND, 1);
    }

    switch (nfc_thread_command) {
    case NFC_THREAD_COMMAND_PASS_THROUGH_SEND:
        nfc_proc_pass_through_send();
        break;
    case NFC_THREAD_COMMAND_READ_T2T:
        nfc_proc_read_t2t();
        break;
    case NFC_THREAD_COMMAND_WRITE_T2T:
        nfc_proc_write_t2t();
        break;
    default:
        break;
    }

    nfc_state = NFC_STATE_COMPLETED;
}

void *gamepad_nfc_process_commands(void *arg)
{
    for (;;) {
        pthread_mutex_lock(&nfc_thread_mtx);
        while (nfc_thread_command == NFC_THREAD_COMMAND_NONE && !is_interrupted()) {
            pthread_cond_wait(&nfc_thread_cond, &nfc_thread_mtx);
        }

        // Check if the thread should quit
        if (nfc_thread_command == NFC_THREAD_COMMAND_QUIT || is_interrupted()) {
            pthread_mutex_unlock(&nfc_thread_mtx);
            break;
        }

        nfc_process_thread_command();

        // notify wii u that command has completed
        set_status_flag(STATUS_NFC_COMMAND_DONE, 1);

        nfc_thread_command = NFC_THREAD_COMMAND_NONE;
        pthread_mutex_unlock(&nfc_thread_mtx);
    }

    return NULL;
}

void gamepad_nfc_set_backend(const VanillaNfcBackend *backend)
{
    nfc_backend = backend;
}

static void nfc_handle_startup(uint8_t *result, const NfcStartupRequest *request)
{
    pthread_mutex_lock(&nfc_thread_mtx);

    switch (nfc_state) {
    default: // We cheat here and allow startup in any state to reset
             // TODO how does the gamepad do this?
    case NFC_STATE_INITIALIZED:
    case NFC_STATE_ANTENNA_CHECKED:
        vanilla_log("Initializing NFC, powerMode %d", request->power_mode);

        // if (nfc_state == NFC_STATE_ANTENNA_CHECKED) {
            set_status_flag(STATUS_NFC_COMMAND_DONE, 0);
        // }

        nfc_state = NFC_STATE_READY;

        // Initialize status flags
        set_status_flag(STATUS_NFC_POWER_MODE, request->power_mode);
        set_status_flag(STATUS_NFC_CRC_DISABLED, 0);
        set_status_flag(STATUS_NFC_MODE, 0);
        set_status_flag(STATUS_NFC_INITIALIZED, 1);
        *result = 0;
        break;
        // default:
        //     vanilla_log("Invalid state");
        //     *result = 2;
        //     break;
    }

    pthread_mutex_unlock(&nfc_thread_mtx);
}

static void nfc_handle_result_check(uint8_t *result, NfcResultCheckResponse *response)
{
    if (nfc_state == NFC_STATE_DISCOVERED) {
        vanilla_log("Invalid state");
        *result = 8;
        return;
    }

    pthread_mutex_lock(&nfc_thread_mtx);

    switch (nfc_state) {
    case NFC_STATE_INITIALIZED:
        vanilla_log("Invalid state");
        *result = 2;
        break;
    case NFC_STATE_COMPLETED:
    case NFC_STATE_ABORTED:
    case NFC_STATE_MULTI_DISCOVERED:
    case NFC_STATE_ANTENNA_CHECKED:
        set_status_flag(STATUS_NFC_COMMAND_DONE, 0);
        set_status_flag(STATUS_NFC_TAG_FOUND, 0);

        if (nfc_state == NFC_STATE_ANTENNA_CHECKED) {
            nfc_state = NFC_STATE_INITIALIZED;
            *result = nfc_result;
            return;
        }

        memcpy(response->uid, nfc_discover_data.uid, sizeof(response->uid));

        nfc_state = NFC_STATE_READY;
        *result = nfc_result;
        break;
    default:
        vanilla_log("Invalid state");
        *result = 8;
        break;
    }

    pthread_mutex_unlock(&nfc_thread_mtx);
}

static void nfc_handle_abort(uint8_t *result)
{
    // don't lock nfc thread, this runs asynchronously

    switch (nfc_state) {
    case NFC_STATE_INITIALIZED:
        vanilla_log("Invalid state");
        *result = 2;
        break;
    case NFC_STATE_DISCOVERY:
    case NFC_STATE_MULTI_DISCOVERY:
        nfc_state = NFC_STATE_ABORTED;
        nfc_backend->abort_discover();
        *result = 0;
        break;
    case NFC_STATE_DISCOVERED:
        nfc_state = NFC_STATE_ABORTED;
        *result = 0;
        break;
    default:
        vanilla_log("Invalid state");
        *result = 6;
        break;
    }
}

static void nfc_handle_shutdown(uint8_t *result)
{
    pthread_mutex_lock(&nfc_thread_mtx);

    switch (nfc_state) {
    case NFC_STATE_ANTENNA_CHECKED:
        set_status_flag(STATUS_NFC_COMMAND_DONE, 0);
        *result = 0;
        break;
    case NFC_STATE_READY:
        nfc_state = NFC_STATE_INITIALIZED;

        set_status_flag(STATUS_NFC_INITIALIZED, 0);
        set_status_flag(STATUS_NFC_POWER_MODE, 0);
        *result = 0;
        break;
    default:
        vanilla_log("Invalid state");
        *result = 7;
        break;
    }

    pthread_mutex_unlock(&nfc_thread_mtx);
}

static void nfc_handle_pass_through_send(uint8_t *result, NfcPassThroughSendRequest *request)
{
    pthread_mutex_lock(&nfc_thread_mtx);

    switch (nfc_state) {
    case NFC_STATE_INITIALIZED:
        vanilla_log("Invalid state");
        *result = 2;
        break;
    case NFC_STATE_READY:
        nfc_discovery_timeout = ntohs(request->discovery_timeout);

        // TODO We only support using pass through send for discovery at the moment
        if (!request->start_discovery) {
            *result = 0x11;
            break;
        }

        nfc_start_thread_command(NFC_THREAD_COMMAND_PASS_THROUGH_SEND);
        *result = 0;
        break;
    default:
        vanilla_log("Invalid state");
        *result = 8;
        break;
    }

    pthread_mutex_unlock(&nfc_thread_mtx);
}

static void nfc_handle_pass_through_receive(uint8_t *result, NfcPassThroughReceiveReponse *response)
{
    pthread_mutex_lock(&nfc_thread_mtx);

    switch (nfc_state) {
    case NFC_STATE_INITIALIZED:
        vanilla_log("Invalid state");
        *result = 2;
        break;
    case NFC_STATE_COMPLETED:
    case NFC_STATE_ABORTED:
        set_status_flag(STATUS_NFC_COMMAND_DONE, 0);

        if (1 /* start_discovery */) {
            // nfc.rpl expects a raw tNFC_ACTIVATE_DEVT struct from the Broadcom nfc stack here
            // lets just fake a struct which works
            response->responseSize = 0x98;
            memset(response->data, 0, 0x98);
            response->data[2] = nfc_discover_data.protocol;
            response->data[3] = NFC_TECHNOLOGY_A;
            response->data[6] = nfc_discover_data.uid_size;
            memcpy(&response->data[7], nfc_discover_data.uid, nfc_discover_data.uid_size);
        }

        nfc_state = NFC_STATE_READY;
        *result = nfc_result;
        break;
    default:
        vanilla_log("Invalid state");
        *result = 8;
        break;
    }

    pthread_mutex_unlock(&nfc_thread_mtx);
}

static void nfc_handle_set_mode(uint8_t *result, uint8_t mode)
{
    pthread_mutex_lock(&nfc_thread_mtx);

    switch (nfc_state) {
    case NFC_STATE_INITIALIZED:
        vanilla_log("Invalid state");
        *result = 2;
        break;
    case NFC_STATE_READY:
        if (mode == 0) {
            set_status_flag(STATUS_NFC_MODE, 0);
            *result = 0;
            break;
        }

        *result = 0xf;
        break;
    default:
        vanilla_log("Invalid state");
        *result = 8;
        break;
    }

    pthread_mutex_unlock(&nfc_thread_mtx);
}

static void nfc_handle_antenna_check(uint8_t *result)
{
    pthread_mutex_lock(&nfc_thread_mtx);

    switch (nfc_state) {
    case NFC_STATE_INITIALIZED:
        // We always fake success for the antenna check
        set_status_flag(STATUS_NFC_COMMAND_DONE, 1);
        nfc_state = NFC_STATE_ANTENNA_CHECKED;
        nfc_result = 0;
        *result = 0;
        break;
    default:
        vanilla_log("Invalid state");
        *result = 0x16;
        break;
    }

    pthread_mutex_unlock(&nfc_thread_mtx);
}

static void nfc_handle_read_t2t_start(uint8_t *result, const NfcReadT2TStartRequest *request)
{
    pthread_mutex_lock(&nfc_thread_mtx);

    switch (nfc_state) {
    case NFC_STATE_INITIALIZED:
        vanilla_log("Invalid state\n");
        *result = 2;
        break;
    case NFC_STATE_READY: {
        nfc_discovery_timeout = ntohs(request->discovery_timeout);

        // Prepare parameters for backend
        memset(&nfc_read_t2t_params, 0, sizeof(nfc_read_t2t_params));
        memcpy(nfc_read_t2t_params.uid, request->uid, sizeof(request->uid));
        memcpy(nfc_read_t2t_params.uid_mask, request->uid_mask, sizeof(request->uid_mask));
        nfc_read_t2t_params.command_timeout = ntohl(request->command_timeout);
        nfc_read_t2t_params.num_ranges = request->num_ranges;
        for (int i = 0; i < request->num_ranges; i++) {
            nfc_read_t2t_params.ranges[i].start = request->ranges[i].start;
            nfc_read_t2t_params.ranges[i].end = request->ranges[i].end;
        }
        nfc_read_t2t_params.perform_pwd_auth = request->pwd_auth;

        nfc_start_thread_command(NFC_THREAD_COMMAND_READ_T2T);
        *result = 0;
        break;
    }
    default:
        vanilla_log("Invalid state\n");
        *result = 8;
        break;
    }

    pthread_mutex_unlock(&nfc_thread_mtx);
}

static void nfc_handle_read_t2t(uint8_t *result, NfcReadT2TResponse *response)
{
    pthread_mutex_lock(&nfc_thread_mtx);

    switch (nfc_state) {
    case NFC_STATE_INITIALIZED:
        vanilla_log("Invalid state\n");
        *result = 2;
        break;
    case NFC_STATE_UNINITIALIZED:
    case NFC_STATE_READY:
        vanilla_log("Invalid state\n");
        *result = 3;
        break;
    case NFC_STATE_COMPLETED:
    case NFC_STATE_ABORTED:
    case NFC_STATE_MULTI_DISCOVERED: {
        set_status_flag(STATUS_NFC_COMMAND_DONE, 0);
        set_status_flag(STATUS_NFC_TAG_FOUND, 0);

        // TODO these are hardcoded for now (is there any case where non t2ts end up here?)
        response->rf_disc_id = 0;
        response->protocol = NFC_PROTOCOL_T2T;
        response->discovery_type = NFC_TECHNOLOGY_A;

        response->uid_size = nfc_discover_data.uid_size;
        memcpy(response->uid, nfc_discover_data.uid, response->uid_size);

        // copy over ranges from start params
        response->num_ranges = nfc_read_t2t_params.num_ranges;
        for (int i = 0; i < nfc_read_t2t_params.num_ranges; i++) {
            response->ranges[i].start = nfc_read_t2t_params.ranges[i].start;
            response->ranges[i].end = nfc_read_t2t_params.ranges[i].end;
        }

        // copy over data
        memcpy(response->data, nfc_read_t2t_data.data, sizeof(response->data));
        memcpy(response->signature, nfc_read_t2t_data.signature, sizeof(response->signature));
        memcpy(response->version, nfc_read_t2t_data.version, sizeof(response->version));

        nfc_state = NFC_STATE_READY;
        *result = nfc_result;
        break;
    }
    default:
        vanilla_log("Invalid state");
        *result = 8;
        break;
    }

    pthread_mutex_unlock(&nfc_thread_mtx);
}

static void nfc_handle_write_t2t(uint8_t *result, NfcWriteT2TRequest *request)
{
    pthread_mutex_lock(&nfc_thread_mtx);

    switch (nfc_state) {
    case NFC_STATE_INITIALIZED:
        *result = 2;
        break;
    case NFC_STATE_READY:
        nfc_discovery_timeout = ntohs(request->discovery_timeout);

        // Prepare parameters for backend
        memset(&nfc_write_t2t_params, 0, sizeof(nfc_write_t2t_params));
        memcpy(nfc_write_t2t_params.uid, request->uid, sizeof(request->uid));
        memcpy(nfc_write_t2t_params.uid_mask, request->uid_mask, sizeof(request->uid_mask));
        memcpy(nfc_write_t2t_params.expected_version, request->version, sizeof(request->version));
        nfc_write_t2t_params.command_timeout = ntohl(request->command_timeout);
        nfc_write_t2t_params.num_ranges = request->num_ranges;
        for (int i = 0; i < request->num_ranges; i++) {
            nfc_write_t2t_params.ranges[i].address = request->ranges[i].address;
            nfc_write_t2t_params.ranges[i].size = request->ranges[i].size;
            memcpy(nfc_write_t2t_params.ranges[i].data, request->ranges[i].data, sizeof(request->ranges[i].data));
        }
        nfc_write_t2t_params.activation_address = request->activation_address;
        memcpy(nfc_write_t2t_params.deactivation_data, request->deactivation_data, sizeof(request->deactivation_data));
        memcpy(nfc_write_t2t_params.activation_data, request->activation_data, sizeof(request->activation_data));

        nfc_write_t2t_params.perform_pwd_auth = request->pwd_auth;
        nfc_write_t2t_params.perform_activation = request->activation;

        nfc_start_thread_command(NFC_THREAD_COMMAND_WRITE_T2T);
        *result = 0;
        break;
    default:
        *result = 8;
        break;
    }

    pthread_mutex_unlock(&nfc_thread_mtx);
}

void gamepad_nfc_control(const uint8_t *request_bytes, uint16_t request_size, uint8_t *response_bytes,
                         uint16_t *response_size)
{
    if (request_size < 1) {
        return;
    }

    NfcRequest *request = (NfcRequest *) request_bytes;
    NfcResponse *response = (NfcResponse *) response_bytes;

    // all commands at least respond with a result code
    *response_size = 1;

    vanilla_log("NFC received command 0x%02x", request->command);

    switch (request->command) {
    case NFC_COMMAND_STARTUP:
        return nfc_handle_startup(&response->result, &request->startup);
    case NFC_COMMAND_RESULT_CHECK:
        *response_size += sizeof(NfcResultCheckResponse);
        return nfc_handle_result_check(&response->result, &response->result_check);
    case NFC_COMMAND_ABORT:
        return nfc_handle_abort(&response->result);
    case NFC_COMMAND_SHUTDOWN:
        return nfc_handle_shutdown(&response->result);
    case NFC_COMMAND_PASS_THROUGH_SEND:
        return nfc_handle_pass_through_send(&response->result, &request->pass_through_send);
    case NFC_COMMAND_PASS_THROUGH_RECEIVE:
        *response_size += sizeof(NfcPassThroughReceiveReponse);
        return nfc_handle_pass_through_receive(&response->result, &response->pass_through_receive);
    case NFC_COMMAND_SET_MODE:
        return nfc_handle_set_mode(&response->result, request->set_mode.mode);
    case NFC_COMMAND_ANTENNA_CHECK:
        return nfc_handle_antenna_check(&response->result);
    case NFC_COMMAND_READ_T2T_START:
        return nfc_handle_read_t2t_start(&response->result, &request->read_t2t_start);
    case NFC_COMMAND_READ_T2T:
        *response_size += sizeof(NfcReadT2TResponse);
        return nfc_handle_read_t2t(&response->result, &response->read_t2t);
    case NFC_COMMAND_WRITE_T2T:
        return nfc_handle_write_t2t(&response->result, &request->write_t2t);

    // Unimplemented commands
    case NFC_COMMAND_READ:
    case NFC_COMMAND_WRITE_START:
    case NFC_COMMAND_FORMAT_START:
    case NFC_COMMAND_SET_READ_ONLY:
    case NFC_COMMAND_IS_TAG_PRESENT:
    case NFC_COMMAND_DETECT_START:
    case NFC_COMMAND_DETECT:
    case NFC_COMMAND_DETECT_START_MULTI:
    case NFC_COMMAND_DETECT_MULTI:
    case NFC_COMMAND_PASS_THROUGH_SEND2:
    case NFC_COMMAND_PASS_THROUGH_RECEIVE2:
        vanilla_log("Unimplemented NFC command 0x%02x", request->command);
        break;

    default:
        vanilla_log("Unknown NFC command 0x%02x", request->command);
        break;
    }

    response->result = 2;
}

int gamepad_nfc_init(void)
{
    if (!nfc_backend) {
        vanilla_log("No NFC backend");
        return -1;
    }

    if (nfc_backend->init() != VANILLA_SUCCESS) {
        vanilla_log("NFC: failed to init backend");
        return -1;
    }

    pthread_create(&nfc_thread, NULL, gamepad_nfc_process_commands, NULL);

#ifndef __APPLE__
    pthread_setname_np(nfc_thread, "vanilla-nfc");
#endif

    nfc_state = NFC_STATE_INITIALIZED;

    return 0;
}

void gamepad_nfc_deinit(void)
{
    if (nfc_state == NFC_STATE_DISCOVERY) {
        nfc_backend->abort_discover();
    }

    nfc_start_thread_command(NFC_THREAD_COMMAND_QUIT);
    pthread_join(nfc_thread, NULL);

    nfc_backend->shutdown();
}
