#ifndef VANILLA_UTIL_H
#define VANILLA_UTIL_H

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define CLAMP(x, min, max) (MIN(MAX(x, min), max))

#define CLEANUP(x) __attribute__((__cleanup__(x)))
static void freep(void *p) {
    if (p)
        free(*(void **) p);
}

size_t read_line_from_fd(int fd, char *output, size_t max_output_size);
size_t read_line_from_file(FILE *file, char *output, size_t max_output_size);
size_t get_max_path_length();

void clear_interrupt();
int is_interrupted();
void force_interrupt();
void install_interrupt_handler();
void uninstall_interrupt_handler();
size_t get_millis();
unsigned int reverse_bits(unsigned int b, int bit_count);

uint16_t crc16(const void* data, size_t len);

#endif // VANILLA_UTIL_H