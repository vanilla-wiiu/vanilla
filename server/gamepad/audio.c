#include "audio.h"

#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>

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

void handle_audio_packet(vanilla_event_handler_t event_handler, void *context, char *data, size_t len)
{
    for (int byte = 0; byte < len; byte++) {
        data[byte] = (unsigned char) reverse_bits(data[byte], 8);
    }

    AudioPacket *ap = (AudioPacket *) data;

    ap->format = reverse_bits(ap->format, 3);
    ap->seq_id = reverse_bits(ap->seq_id, 10);
    ap->payload_size = reverse_bits(ap->payload_size, 16);
    ap->timestamp = reverse_bits(ap->timestamp, 32);
    for (int byte = 0; byte < ap->payload_size; ++byte) {
        ap->payload[byte] = (unsigned char) reverse_bits(ap->payload[byte], 8);
    }

    if (ap->type == TYPE_VIDEO) {
        AudioPacketVideoFormat *avp = (AudioPacketVideoFormat *) ap->payload;
        avp->timestamp = ntohl(avp->timestamp);
        avp->video_format = ntohl(avp->video_format);

        // TODO: Implement
        return;
    }

    if (ap->vibrate) {
        event_handler(context, VANILLA_EVENT_VIBRATE, NULL, ap->payload_size);
    }
    
    event_handler(context, VANILLA_EVENT_AUDIO, ap->payload, ap->payload_size);
}

void *listen_audio(void *x)
{
    struct gamepad_thread_context *info = (struct gamepad_thread_context *) x;
    unsigned char data[2048];
    ssize_t size;
    do {
        size = recv(info->socket_aud, data, sizeof(data), 0);
        if (size > 0) {
            if (size == 4 && *(uint32_t *)data == 0xCAFEBABE) break;
            handle_audio_packet(info->event_handler, info->context, data, size);
        }
    } while (!is_interrupted());
    
    pthread_exit(NULL);

    return NULL;
}