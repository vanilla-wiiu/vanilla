#ifndef DHCP_H
#define DHCP_H

#include <stdint.h>
#include <netinet/in.h>

#define SDHCP_MAX_DNS 4

typedef struct {
    struct in_addr yiaddr;
    struct in_addr subnet_mask;
    struct in_addr router;
    struct in_addr dns[SDHCP_MAX_DNS];
    int dns_count;
    uint32_t lease_time;         /* host byte order */
    uint32_t server_identifier;  /* network byte order */
} sdhcp_lease_t;

// Simple one-shot DHCP acquisition for connection to Wii U
//
// Return 0 on success, -1 on error with errno set
int sdhcp_acquire(const char *ifname, int timeout_ms, int retries, sdhcp_lease_t *lease);

#endif
