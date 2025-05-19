#include "audio.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

#include "gamepad.h"
#include "vanilla.h"
#include "util.h"

#pragma pack(push, 1)
typedef struct {
    unsigned format : 3;
    unsigned mono : 1;
    unsigned vibrate : 1;
    unsigned type : 1;
    unsigned seq_id : 10;
    unsigned payload_size : 16;
    unsigned timestamp : 32;
    unsigned char payload[2048];
} AudioPacket;
const static unsigned int TYPE_AUDIO = 0;
const static unsigned int TYPE_VIDEO = 1;
#pragma pack(pop)

typedef struct {
    uint32_t timestamp;
    uint32_t unknown_freq_0[2];
    uint32_t unknown_freq_1[2];
    uint32_t video_format;
} AudioPacketVideoFormat;

static unsigned char queued_audio[2048];
static size_t queued_audio_start = 0;
static size_t queued_audio_end = 0;
static pthread_mutex_t queued_audio_mutex;

int send_audio_packet(const void *data, size_t len)
{
    pthread_mutex_lock(&queued_audio_mutex);

    for (size_t i = 0; i < len; ) {
        size_t phys = queued_audio_end % sizeof(queued_audio);
        size_t max_write = MIN(sizeof(queued_audio) - phys, len - i);
        memcpy(queued_audio + phys, ((const unsigned char *) data) + i, max_write);
        i += max_write;
        queued_audio_end += max_write;
    }

    pthread_mutex_unlock(&queued_audio_mutex);

    return VANILLA_SUCCESS;
}

static void handle_queued_audio(gamepad_context_t *ctx, int seq_id)
{
    AudioPacket ap;

    // Initialize payload size to zero
    const size_t MIC_PAYLOAD_SIZE = 512;
    ap.payload_size = 0;

    // Lock mutex
    pthread_mutex_lock(&queued_audio_mutex);

    // If available, copy over to our packet
    if (queued_audio_end >= (queued_audio_start + MIC_PAYLOAD_SIZE)) {
        for (size_t i = 0; i < MIC_PAYLOAD_SIZE; ) {
            size_t phys = queued_audio_start % sizeof(queued_audio);
            size_t max_write = MIN(sizeof(queued_audio) - phys, MIC_PAYLOAD_SIZE - i);
            memcpy(ap.payload + i, queued_audio + queued_audio_start, max_write);
            i += max_write;
            queued_audio_start += max_write;
        }
        ap.payload_size = MIC_PAYLOAD_SIZE;
    }

    // Unlock
    pthread_mutex_unlock(&queued_audio_mutex);

    // If we didn't get audio, return here
    if (!ap.payload_size) {
        return;
    }

    // struct timeval tv;
    // gettimeofday(&tv, 0);
    // if ((tv.tv_sec % 2) == 1) {
        // for (int i = 0; i < ap.payload_size; i++) {
        //     ap.payload[i] = (rand() % 0xFF);
        // }
        // vanilla_log("NOISE!");
    // } else {
    //     memset(ap.payload, 0x00, ap.payload_size);
    //     vanilla_log("...quiet...");
    // }

    // Set up remaining default parameters
    ap.format = 6;
    ap.mono = 1;
    ap.vibrate = 0;
    ap.type = TYPE_AUDIO; // Audio data
    ap.seq_id = seq_id + 1;
    ap.timestamp = 0;

    // Reverse bits on params
    ap.format = reverse_bits(ap.format, 3);
    ap.seq_id = reverse_bits(ap.seq_id, 10);
    ap.payload_size = reverse_bits(ap.payload_size, 16);//ntohs(ap.payload_size);
    // ap.timestamp = reverse_bits(ap.timestamp, 32); // Not necessary because timestamp is 0

    // Further reverse bits
    unsigned char *bytes = (unsigned char *) &ap;
    const size_t header_sz = sizeof(AudioPacket) - sizeof(ap.payload);
    for (int i = 0; i < header_sz; i++) {
        bytes[i] = (unsigned char) reverse_bits(bytes[i], 8);
    }

    // Send packet to console
    send_to_console(ctx->socket_aud, &ap, header_sz + ap.payload_size, PORT_AUD);
}

void handle_audio_packet(gamepad_context_t *ctx, unsigned char *data, size_t len)
{
    //
    // === IMPORTANT NOTE! ===
    //
    // This for loop skips ap->format, ap->seq_id, and ap->timestamp to save processing.
    // If you want those, you'll have to adjust this loop.
    //
    for (int byte = 0; byte < 8; byte++) {
        data[byte] = (unsigned char) reverse_bits(data[byte], 8);
    }

    AudioPacket *ap = (AudioPacket *) data;

    ap->format = reverse_bits(ap->format, 3);
    ap->seq_id = reverse_bits(ap->seq_id, 10);
    ap->payload_size = reverse_bits(ap->payload_size, 16);//ntohs(ap->payload_size);
    ap->timestamp = reverse_bits(ap->timestamp, 32);

    if (ap->type == TYPE_VIDEO) {
        AudioPacketVideoFormat *avp = (AudioPacketVideoFormat *) ap->payload;
        avp->timestamp = ntohl(avp->timestamp);
        avp->video_format = ntohl(avp->video_format);

        // TODO: Implement
        return;
    }

    if (ap->payload_size) {
        push_event(ctx->event_loop, VANILLA_EVENT_AUDIO, ap->payload, ap->payload_size);
    }

    uint8_t vibrate_val = ap->vibrate;
    push_event(ctx->event_loop, VANILLA_EVENT_VIBRATE, &vibrate_val, sizeof(vibrate_val));

    handle_queued_audio(ctx, ap->seq_id);
}

void *listen_audio(void *x)
{
    gamepad_context_t *info = (gamepad_context_t *) x;
    unsigned char data[2048];
    ssize_t size;

    queued_audio_start = 0;
    queued_audio_end = 0;

    pthread_mutex_init(&queued_audio_mutex, 0);

    do {
        size = recv(info->socket_aud, data, sizeof(data), 0);
        if (size > 0) {
            handle_audio_packet(info, data, size);
        }
    } while (!is_interrupted());

    pthread_mutex_destroy(&queued_audio_mutex);

    pthread_exit(NULL);

    return NULL;
}
