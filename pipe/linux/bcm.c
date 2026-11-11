#include "bcm.h"

#include <errno.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define VANILLA_WLC_SET_PM 86
#define VANILLA_WLC_IOCTL_MAGIC 0x14e46c77

struct vanilla_bcm_ioctl {
    uint32_t cmd;
    void *buf;
    uint32_t len;
    uint8_t set;
    uint8_t padding[3];
    uint32_t used;
    uint32_t needed;
    uint32_t driver;
};

int vanilla_bcm_set_pm(const char *interface, int value)
{
    int skt;
    int ret = -1;
    int saved_errno;
    int attempt;
    struct ifreq ifr;
    struct vanilla_bcm_ioctl ioc;

    memset(&ifr, 0, sizeof(ifr));
    memset(&ioc, 0, sizeof(ioc));

    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);

    ioc.cmd = VANILLA_WLC_SET_PM;
    ioc.buf = &value;
    ioc.len = sizeof(value);
    ioc.set = 1;
    ioc.driver = VANILLA_WLC_IOCTL_MAGIC;

    ifr.ifr_data = (void *)&ioc;

    skt = socket(AF_INET, SOCK_DGRAM, 0);
    if (skt < 0)
        return -errno;

    for (attempt = 0; attempt < 5; attempt++) {
        ret = ioctl(skt, SIOCDEVPRIVATE, &ifr);
        if (ret == 0 || errno != EINTR)
            break;

        usleep(100000);
    }

    saved_errno = errno;
    close(skt);

    if (ret < 0)
        return -saved_errno;

    return 0;
}
