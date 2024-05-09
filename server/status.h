#ifndef VANILLA_SERVER_STATUS_H
#define VANILLA_SERVER_STATUS_H

#define VANILLA_SUCCESS              0
#define VANILLA_ERROR               -1
#define VANILLA_READY               -2
#define VANILLA_INFO                -3
#define VANILLA_UNKNOWN_COMMAND     -4
#define VANILLA_INVALID_ARGUMENT    -5

void print_status(int errno);
void print_info(const char *str, ...);

#endif // VANILLA_SERVER_STATUS_H