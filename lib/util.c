#include "util.h"

#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "status.h"
#include "vanilla.h"

// TODO: Static variables are undesirable
int interrupted = 0;

void interrupt_handler(int signum)
{
    print_info("INTERRUPT SIGNAL RECEIVED, CANCELLING...");
    interrupted = 1;
}

int is_interrupted()
{
    return interrupted;
}

void force_interrupt()
{
    interrupted = 1;
}

void clear_interrupt()
{
    interrupted = 0;
}

void install_interrupt_handler()
{
    clear_interrupt();
    signal(SIGINT, interrupt_handler);
}

void uninstall_interrupt_handler()
{
    signal(SIGINT, SIG_DFL);
}

uint16_t crc16(const void *data, size_t len)
{
    const uint8_t *src = data;
    uint16_t crc = 0xffff;

    if (len == 0) {
        return crc;
    }

    while (len--) {
        crc ^= *src++;
        for (int i = 0; i < 8; i++) {
            uint16_t mult = (crc & 1) ? 0x8408 : 0;
            crc = (crc >> 1) ^ mult;
        }
    }

    return crc;
}

size_t get_millis()
{
    size_t            ms; // Milliseconds
    size_t          s;  // Seconds
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    s  = spec.tv_sec;
    ms = round(spec.tv_nsec / 1.0e6); // Convert nanoseconds to milliseconds
    if (ms > 999) {
        s++;
        ms = 0;
    }

    return (s * 1000) + ms;
}

uint32_t reverse_bits(uint32_t b, int bit_count)
{
    uint32_t mask = 0b11111111111111110000000000000000;
    b = (b & mask) >> 16 | (b & ~mask) << 16;
    mask = 0b11111111000000001111111100000000;
    b = (b & mask) >> 8 | (b & ~mask) << 8;
    mask = 0b11110000111100001111000011110000;
    b = (b & mask) >> 4 | (b & ~mask) << 4;
    mask = 0b11001100110011001100110011001100;
    b = (b & mask) >> 2 | (b & ~mask) << 2;
    mask = 0b10101010101010101010101010101010;
    b = (b & mask) >> 1 | (b & ~mask) << 1;

    b >>= 32 - bit_count;
    return b;
}