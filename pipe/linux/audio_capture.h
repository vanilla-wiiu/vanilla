#ifndef VANILLA_AUDIO_CAPTURE_H
#define VANILLA_AUDIO_CAPTURE_H

#include <stdint.h>
#include <stddef.h>

// Initialize audio capture device
// Returns 0 on success, negative on failure 
int audio_capture_init();

// Read audio samples from microphone
// Samples should be 16-bit PCM at 48kHz
// Returns number of bytes read or negative on error
int audio_capture_read(int16_t* buffer, size_t buffer_size);

// Clean up audio capture resources
void audio_capture_cleanup();

#endif // VANILLA_AUDIO_CAPTURE_H 