#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vanilla_connection {
    int sock;           // TCP socket
    int connected;
} vanilla_connection_t;

#ifdef __cplusplus
}
#endif
