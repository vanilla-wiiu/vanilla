#ifndef VPI_GAME_DECODE_H
#define VPI_GAME_DECODE_H

#include <libavutil/frame.h>

extern pthread_mutex_t vpi_decode_loop_mutex;
extern pthread_cond_t vpi_decode_loop_cond;
extern int vpi_decode_loop_running;
extern uint8_t vpi_decode_data[65536];
extern uint8_t vpi_decode_ready;
extern size_t vpi_decode_size;
extern AVFrame *vpi_present_frame;
extern pthread_mutex_t vpi_decoding_mutex;

void *vpi_decode_loop(void *);

#endif // VPI_GAME_DECODE_H
