#include "audio.h"

#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>

#include "gamepad.h"
#include "status.h"
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

void handle_audio_packet(gamepad_context_t *ctx, unsigned char *data, size_t len)
{
    //
    // === IMPORTANT NOTE! ===
    //
    // This for loop skips ap->format, ap->seq_id, and ap->timestamp to save processing.
    // If you want those, you'll have to adjust this loop.
    //
    for (int byte = 0; byte < 4; byte++) {
        data[byte] = (unsigned char) reverse_bits(data[byte], 8);
    }

    AudioPacket *ap = (AudioPacket *) data;

    // ap->format = reverse_bits(ap->format, 3);
    // ap->seq_id = reverse_bits(ap->seq_id, 10);
    ap->payload_size = reverse_bits(ap->payload_size, 16);
    // ap->timestamp = reverse_bits(ap->timestamp, 32);

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
}

void *listen_audio(void *x)
{
    gamepad_context_t *info = (gamepad_context_t *) x;
    unsigned char data[2048];
    ssize_t size;
    do {
        size = recv(info->socket_aud, data, sizeof(data), 0);
        if (size > 0) {
            if (is_stop_code(data, size)) break;
            handle_audio_packet(info, data, size);
        }
    } while (!is_interrupted());
    
    pthread_exit(NULL);

    return NULL;
}