#pragma once

struct sysinfo {
    long uptime;
    unsigned long totalram;
    unsigned long freeram;
    unsigned short procs;
};

/* Not supported on iOS */
static inline int sysinfo(struct sysinfo *info)
{
    if (info) {
        info->uptime = 0;
        info->totalram = 0;
        info->freeram = 0;
        info->procs = 0;
    }
    return -1;
}
