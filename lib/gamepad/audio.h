#ifndef GAMEPAD_AUDIO_H
#define GAMEPAD_AUDIO_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

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

void *listen_audio(void *x);
int send_audio_packet(const void *data, size_t len);

#endif // GAMEPAD_AUDIO_H
