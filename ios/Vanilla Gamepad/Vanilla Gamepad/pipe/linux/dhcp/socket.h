/* socket.h */
#ifndef _SOCKET_H
#define _SOCKET_H

int read_interface(char *interface, int *ifindex, uint32_t *addr, uint8_t *arp);
int listen_socket(uint32_t ip, int port, char *inf);

#endif
