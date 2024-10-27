#include "util.h"

#include <dlfcn.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
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

static int (*ppthread_setname_np)(pthread_t, const char*) = NULL;
static int checked_for_pthread_setname_np = 0;
void name_thread(pthread_t thread, const char *name)
{
    if (!checked_for_pthread_setname_np) {
        void *fn = dlsym(RTLD_DEFAULT, "pthread_setname_np");
        ppthread_setname_np = (int(*)(pthread_t, const char*)) fn;
        checked_for_pthread_setname_np = 1;
    }

    if (ppthread_setname_np) {
        ppthread_setname_np(thread, name);
    }
}