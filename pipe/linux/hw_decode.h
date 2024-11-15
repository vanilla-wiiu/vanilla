#ifndef VANILLA_HW_DECODE_H
#define VANILLA_HW_DECODE_H

#include <stddef.h>
#include <stdint.h>

// Initialize hardware decoder
// Returns 0 on success, negative on failure
int hw_decode_init();

// Decode a frame using hardware acceleration
// Input: H.264 NAL unit data
// Output: Raw YUV420 frame data
// Returns: Number of bytes written to output buffer, or negative on error
int hw_decode_frame(const uint8_t* input, size_t input_size, 
                   uint8_t* output, size_t output_size);

// Clean up hardware decoder resources
void hw_decode_cleanup();

#endif // VANILLA_HW_DECODE_H 