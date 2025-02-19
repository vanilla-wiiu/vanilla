#ifndef GAMEPAD_VIDEO_H
#define GAMEPAD_VIDEO_H

#include <stdint.h>
#include <stdlib.h>

void *listen_video(void *x);
void request_idr();
size_t generate_sps_params(void *data, size_t size);
size_t generate_pps_params(void *data, size_t size);
size_t generate_h264_header(void *data, size_t size);
void write_bits(void *data, size_t size, size_t *bit_index, uint8_t value, size_t bit_width);
void write_exp_golomb(void *data, size_t buffer_size, size_t *bit_index, uint64_t value);

#endif // GAMEPAD_VIDEO_H