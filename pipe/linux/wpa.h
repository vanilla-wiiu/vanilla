#ifndef VANILLA_WPA_H
#define VANILLA_WPA_H

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

void pprint(const char *fmt, ...);
void vanilla_listen(const char *wireless_interface);

#endif // VANILLA_WPA_H
