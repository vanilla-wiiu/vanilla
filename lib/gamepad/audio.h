#ifndef GAMEPAD_AUDIO_H
#define GAMEPAD_AUDIO_H

#include <stddef.h>

void *listen_audio(void *x);
int send_audio_packet(const void *data, size_t len);

#endif // GAMEPAD_AUDIO_H