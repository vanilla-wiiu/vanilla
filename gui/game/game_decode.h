#ifndef VPI_GAME_DECODE_H
#define VPI_GAME_DECODE_H

#include <libavutil/frame.h>
#include <pthread.h>

extern pthread_mutex_t vpi_decode_loop_mutex;
extern pthread_cond_t vpi_decode_loop_cond;
extern int vpi_decode_loop_running;
extern uint8_t vpi_decode_data[65536];
extern uint8_t vpi_decode_ready;
extern size_t vpi_decode_size;
extern AVFrame *vpi_present_frame;
extern pthread_mutex_t vpi_decoding_mutex;
extern pthread_cond_t decoding_wait_cond;

void *vpi_decode_loop(void *);

void vpi_decode_screenshot(const char *filename);
int vpi_decode_is_recording();
int vpi_decode_record(const char *filename);
void vpi_decode_record_stop();
void vpi_decode_send_audio(const void *data, size_t size);

#endif // VPI_GAME_DECODE_H
