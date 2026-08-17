#include "nfc_file.h"
#include "gamepad/nfc.h"
#include "ntag_util.h"
#include "vanilla.h"

#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define NFC_TOUCH_SECONDS 3
#define NTAG_UID_SIZE 7

static volatile int is_aborted = 0;
static pthread_mutex_t touch_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct timespec touch_time;
static char tag_path[PATH_MAX];

static bool nfc_file_touched(void)
{
    struct timespec current;
    clock_gettime(CLOCK_MONOTONIC, &current);

    // Check if touched
    pthread_mutex_lock(&touch_mutex);
    bool touched = current.tv_sec < touch_time.tv_sec ||
                   (current.tv_sec == touch_time.tv_sec && current.tv_nsec <= touch_time.tv_nsec);
    pthread_mutex_unlock(&touch_mutex);

    return touched;
}

void nfc_file_touch_tag(const char *path)
{
    pthread_mutex_lock(&touch_mutex);

    strncpy(tag_path, path, sizeof(tag_path));

    struct timespec current;
    clock_gettime(CLOCK_MONOTONIC, &current);
    touch_time.tv_sec = current.tv_sec + NFC_TOUCH_SECONDS;
    touch_time.tv_nsec = current.tv_nsec;

    pthread_mutex_unlock(&touch_mutex);
}

static bool nfc_file_read_uid(uint8_t uid[NTAG_UID_SIZE])
{
    pthread_mutex_lock(&touch_mutex);
    FILE *f = fopen(tag_path, "rb");
    pthread_mutex_unlock(&touch_mutex);

    if (!f) {
        vanilla_log("Failed to open tag file");
        return false;
    }

    // Read UID from first block
    if (fread(uid, 1, NTAG_UID_SIZE, f) != NTAG_UID_SIZE) {
        vanilla_log("Failed to read UID from tag file");
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

static int nfc_file_init(void)
{
    return VANILLA_SUCCESS;
}

static int nfc_file_shutdown(void)
{
    is_aborted = 1;

    return VANILLA_SUCCESS;
}

static int nfc_file_discover(uint16_t timeout, uint8_t tech_mask, VanillaNfcDiscoverData *data)
{
    vanilla_log("nfc_discover");

    is_aborted = 0;

    struct timespec start, current;
    uint64_t elapsed_ms;
    clock_gettime(CLOCK_MONOTONIC, &start);

    do {
        if ((tech_mask & NFC_TECHNOLOGY_MASK_A) && nfc_file_touched()) {
            // TODO not every tag will be a T2T
            data->protocol = NFC_PROTOCOL_T2T;

            data->uid_size = NTAG_UID_SIZE;
            if (!nfc_file_read_uid(data->uid)) {
                continue;
            }

            return VANILLA_SUCCESS;
        }

        clock_gettime(CLOCK_MONOTONIC, &current);
        elapsed_ms = (current.tv_sec - start.tv_sec) * 1000 + (current.tv_nsec - start.tv_nsec) / 1000000;
    } while ((timeout == 0 || elapsed_ms < timeout) && !is_aborted);

    vanilla_log("nfc_discover timed out");

    return VANILLA_ERR_TIMEOUT;
}

static void nfc_file_abort_discover(void)
{
    is_aborted = 1;
}

static uint8_t nfc_file_read_t2t(const VanillaNfcReadT2TParams *params, VanillaNfcReadT2TData *data)
{
    uint8_t uid[NTAG_UID_SIZE];
    if (!nfc_file_read_uid(uid)) {
        return 0xd;
    }

    // Perform UID check of current selected tag
    for (int i = 0; i < NTAG_UID_SIZE; i++) {
        if ((uid[i] & params->uid_mask[i]) != params->uid[i]) {
            vanilla_log("uid mismatch");
            return 0xa;
        }
    }

    // Just use a zeroed signature, it's unchecked anyways
    memset(data->signature, 0, sizeof(data->signature));
    memset(data->version, 0, sizeof(data->version));

    pthread_mutex_lock(&touch_mutex);
    FILE *f = fopen(tag_path, "rb");
    pthread_mutex_unlock(&touch_mutex);

    if (!f) {
        vanilla_log("Failed to open tag file");
        return 0xd;
    }

    // Read data ranges
    uint8_t *p_data = data->data;
    for (int i = 0; i < params->num_ranges; i++) {
        uint8_t end = params->ranges[i].end;

        // Ignore auth related pages at the end for now
        if (end > 0x84) {
            end = 0x84;
        }

        fseek(f, params->ranges[i].start * NTAG_PAGE_SIZE, SEEK_SET);

        const size_t num_bytes = (end - params->ranges[i].start + 1) * NTAG_PAGE_SIZE;
        if (fread(p_data, 1, num_bytes, f) != num_bytes) {
            vanilla_log("Failed to read tag file");
            fclose(f);
            return 0x3;
        }

        p_data += num_bytes;
    }

    fclose(f);
    return 0;
}

static uint8_t nfc_file_write_t2t(const VanillaNfcWriteT2TParams *params)
{
    uint8_t uid[NTAG_UID_SIZE];
    if (!nfc_file_read_uid(uid)) {
        return 0xd;
    }

    // Perform UID check of current selected tag
    for (int i = 0; i < NTAG_UID_SIZE; i++) {
        if ((uid[i] & params->uid_mask[i]) != params->uid[i]) {
            vanilla_log("uid mismatch");
            return 0xa;
        }
    }

    pthread_mutex_lock(&touch_mutex);
    FILE *f = fopen(tag_path, "rb+");
    pthread_mutex_unlock(&touch_mutex);

    if (!f) {
        vanilla_log("Failed to open tag file");
        return 0xd;
    }

    // Write ranges
    for (int i = 0; i < params->num_ranges; i++) {
        const uint8_t num_bytes = params->ranges[i].size;

        fseek(f, params->ranges[i].address * NTAG_PAGE_SIZE, SEEK_SET);
        if (fwrite(params->ranges[i].data, 1, num_bytes, f) != num_bytes) {
            vanilla_log("Failed to write tag file");
            fclose(f);
            return 0x3;
        }
    }

    // Write activation data
    if (params->perform_activation) {
        fseek(f, params->activation_address * NTAG_PAGE_SIZE, SEEK_SET);
        if (fwrite(params->activation_data, 1, sizeof(params->activation_data), f) != sizeof(params->activation_data)) {
            vanilla_log("Failed to activation data");
            fclose(f);
            return 0x17;
        }
    }

    fclose(f);
    return 0;
}

VanillaNfcBackend nfc_file_backend = {
    .init = nfc_file_init,
    .shutdown = nfc_file_shutdown,
    .discover = nfc_file_discover,
    .abort_discover = nfc_file_abort_discover,

    .read_t2t = nfc_file_read_t2t,
    .write_t2t = nfc_file_write_t2t,
};
