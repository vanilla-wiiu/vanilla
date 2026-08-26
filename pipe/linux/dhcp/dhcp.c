#define _DEFAULT_SOURCE

#include "dhcp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67

#define DHCPDISCOVER 1
#define DHCPOFFER    2
#define DHCPREQUEST  3
#define DHCPACK      5
#define DHCPNAK      6

#define DHCP_MAGIC_COOKIE 0x63825363u
#define DHCP_FLAGS_BROADCAST 0x8000

#define DHCP_OPTION_SUBNET_MASK       1
#define DHCP_OPTION_ROUTER            3
#define DHCP_OPTION_DNS               6
#define DHCP_OPTION_REQUESTED_IP      50
#define DHCP_OPTION_LEASE_TIME        51
#define DHCP_OPTION_MSG_TYPE          53
#define DHCP_OPTION_SERVER_IDENTIFIER 54
#define DHCP_OPTION_PARAMETER_REQUEST 55
#define DHCP_OPTION_END               255
#define DHCP_OPTION_PAD               0

#define DHCP_HTYPE_ETHERNET 1
#define DHCP_HLEN_ETHERNET  6
#define DHCP_OP_BOOTREQUEST 1
#define DHCP_OP_BOOTREPLY   2

#define DHCP_OPTIONS_SIZE 312
#define DHCP_MIN_SIZE     300
#define PKT_BUF_SIZE      1500

#pragma pack(push, 1)
typedef struct {
    uint8_t  op;
    uint8_t  htype;
    uint8_t  hlen;
    uint8_t  hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];
    uint32_t magic_cookie;
    uint8_t  options[DHCP_OPTIONS_SIZE];
} dhcp_packet_t;

typedef struct {
    uint16_t src;
    uint16_t dst;
    uint16_t len;
    uint16_t check;
} sdhcp_udp_header_t;
#pragma pack(pop)

static uint16_t ip_checksum(const void *data, size_t len)
{
    const uint8_t *p = data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += ((uint16_t)p[0] << 8) | p[1];
        p += 2;
        len -= 2;
    }
    if (len) {
        sum += ((uint16_t)p[0] << 8);
    }
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

static uint32_t make_xid(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)ts.tv_nsec ^ (uint32_t)ts.tv_sec ^ (uint32_t)getpid();
}

static int get_mac(const char *ifname, uint8_t mac[6])
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
        close(fd);
        return -1;
    }

    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
    close(fd);
    return 0;
}

static int open_sock(const char *ifname, int *ifindex_out)
{
    int ifindex = if_nametoindex(ifname);
    if (!ifindex) {
        errno = ENODEV;
        return -1;
    }

    int fd = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
    if (fd < 0) return -1;

    struct sockaddr_ll addr;
    memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_IP);
    addr.sll_ifindex = ifindex;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    *ifindex_out = ifindex;
    return fd;
}

static size_t dhcp_init(dhcp_packet_t *d, uint32_t xid, const uint8_t mac[6], uint8_t msg_type)
{
    memset(d, 0, sizeof(*d));
    d->op = DHCP_OP_BOOTREQUEST;
    d->htype = DHCP_HTYPE_ETHERNET;
    d->hlen = DHCP_HLEN_ETHERNET;
    d->xid = htonl(xid);
    d->flags = htons(DHCP_FLAGS_BROADCAST);
    memcpy(d->chaddr, mac, 6);
    d->magic_cookie = htonl(DHCP_MAGIC_COOKIE);

    size_t o = 0;
    d->options[o++] = DHCP_OPTION_MSG_TYPE;
    d->options[o++] = 1;
    d->options[o++] = msg_type;

    return o;
}

static size_t build_discover(dhcp_packet_t *d, uint32_t xid, const uint8_t mac[6])
{
    size_t o = dhcp_init(d, xid, mac, DHCPDISCOVER);

    uint8_t prl[] = {
        DHCP_OPTION_SUBNET_MASK,
        DHCP_OPTION_ROUTER,
        DHCP_OPTION_DNS,
        DHCP_OPTION_LEASE_TIME,
        DHCP_OPTION_SERVER_IDENTIFIER
    };

    d->options[o++] = DHCP_OPTION_PARAMETER_REQUEST;
    d->options[o++] = sizeof(prl);
    memcpy(&d->options[o], prl, sizeof(prl));
    o += sizeof(prl);

    d->options[o++] = DHCP_OPTION_END;

    size_t len = offsetof(dhcp_packet_t, options) + o;
    return len < DHCP_MIN_SIZE ? DHCP_MIN_SIZE : len;
}

static size_t build_request(dhcp_packet_t *d, uint32_t xid, const uint8_t mac[6],
                            uint32_t req_ip, uint32_t server_id)
{
    size_t o = dhcp_init(d, xid, mac, DHCPREQUEST);

    d->options[o++] = DHCP_OPTION_REQUESTED_IP;
    d->options[o++] = 4;
    memcpy(&d->options[o], &req_ip, 4);
    o += 4;

    d->options[o++] = DHCP_OPTION_SERVER_IDENTIFIER;
    d->options[o++] = 4;
    memcpy(&d->options[o], &server_id, 4);
    o += 4;

    uint8_t prl[] = {
        DHCP_OPTION_SUBNET_MASK,
        DHCP_OPTION_ROUTER,
        DHCP_OPTION_DNS,
        DHCP_OPTION_LEASE_TIME
    };

    d->options[o++] = DHCP_OPTION_PARAMETER_REQUEST;
    d->options[o++] = sizeof(prl);
    memcpy(&d->options[o], prl, sizeof(prl));
    o += sizeof(prl);

    d->options[o++] = DHCP_OPTION_END;

    size_t len = offsetof(dhcp_packet_t, options) + o;
    return len < DHCP_MIN_SIZE ? DHCP_MIN_SIZE : len;
}

static int build_ip_udp(uint8_t *buf, size_t buflen,
                        const dhcp_packet_t *dhcp, size_t dhcp_len,
                        size_t *out_len)
{
    size_t total = sizeof(struct iphdr) + sizeof(sdhcp_udp_header_t) + dhcp_len;
    if (buflen < total) {
        errno = EMSGSIZE;
        return -1;
    }

    struct iphdr *ip = (struct iphdr *)buf;
    sdhcp_udp_header_t *udp = (sdhcp_udp_header_t *)(buf + sizeof(struct iphdr));
    uint8_t *payload = buf + sizeof(struct iphdr) + sizeof(sdhcp_udp_header_t);

    memset(buf, 0, sizeof(struct iphdr) + sizeof(sdhcp_udp_header_t));
    memcpy(payload, dhcp, dhcp_len);

    ip->version = 4;
    ip->ihl = 5;
    ip->ttl = 64;
    ip->protocol = IPPROTO_UDP;
    ip->tot_len = htons(total);
    ip->saddr = htonl(INADDR_ANY);
    ip->daddr = htonl(INADDR_BROADCAST);
    ip->check = htons(ip_checksum(ip, sizeof(*ip)));

    udp->src = htons(DHCP_CLIENT_PORT);
    udp->dst = htons(DHCP_SERVER_PORT);
    udp->len = htons(sizeof(sdhcp_udp_header_t) + dhcp_len);
    udp->check = 0; /* IPv4 allows zero UDP checksum */

    *out_len = total;
    return 0;
}

static int send_buf(int fd, int ifindex, const void *buf, size_t len)
{
    struct sockaddr_ll dst;
    memset(&dst, 0, sizeof(dst));
    dst.sll_family = AF_PACKET;
    dst.sll_protocol = htons(ETH_P_IP);
    dst.sll_ifindex = ifindex;
    dst.sll_halen = ETH_ALEN;
    memset(dst.sll_addr, 0xff, ETH_ALEN);

    ssize_t n = sendto(fd, buf, len, 0, (struct sockaddr *)&dst, sizeof(dst));
    return n == (ssize_t)len ? 0 : -1;
}

static int parse_options(const dhcp_packet_t *d, size_t len,
                         uint8_t *msg_type, sdhcp_lease_t *lease)
{
    size_t base = offsetof(dhcp_packet_t, options);
    if (len < base) return -1;
    if (ntohl(d->magic_cookie) != DHCP_MAGIC_COOKIE) return -1;

    *msg_type = 0;
    lease->subnet_mask.s_addr = 0;
    lease->router.s_addr = 0;
    lease->dns_count = 0;
    lease->lease_time = 0;
    lease->server_identifier = 0;

    size_t i = 0, max = len - base;
    while (i < max) {
        uint8_t code = d->options[i++];
        if (code == DHCP_OPTION_PAD) continue;
        if (code == DHCP_OPTION_END) break;
        if (i >= max) return -1;

        uint8_t optlen = d->options[i++];
        if ((size_t)optlen > max - i) return -1;

        const uint8_t *v = &d->options[i];

        switch (code) {
            case DHCP_OPTION_MSG_TYPE:
                if (optlen == 1) *msg_type = v[0];
                break;
            case DHCP_OPTION_SUBNET_MASK:
                if (optlen == 4) memcpy(&lease->subnet_mask.s_addr, v, 4);
                break;
            case DHCP_OPTION_ROUTER:
                if (optlen >= 4) memcpy(&lease->router.s_addr, v, 4);
                break;
            case DHCP_OPTION_DNS: {
                int count = optlen / 4;
                if (count > SDHCP_MAX_DNS) count = SDHCP_MAX_DNS;
                for (int j = 0; j < count; j++) {
                    memcpy(&lease->dns[j].s_addr, v + j * 4, 4);
                }
                lease->dns_count = count;
                break;
            }
            case DHCP_OPTION_LEASE_TIME:
                if (optlen == 4) {
                    uint32_t t;
                    memcpy(&t, v, 4);
                    lease->lease_time = ntohl(t);
                }
                break;
            case DHCP_OPTION_SERVER_IDENTIFIER:
                if (optlen == 4) memcpy(&lease->server_identifier, v, 4);
                break;
        }

        i += optlen;
    }

    return 0;
}

static int recv_dhcp(int fd, uint32_t xid, const uint8_t mac[6], int timeout_ms,
                     dhcp_packet_t *out, size_t *out_len, uint8_t *msg_type,
                     sdhcp_lease_t *lease)
{
    uint8_t buf[PKT_BUF_SIZE];
    struct timeval start, now;

    gettimeofday(&start, NULL);

    for (;;) {
        gettimeofday(&now, NULL);
        long elapsed = (now.tv_sec - start.tv_sec) * 1000L +
                       (now.tv_usec - start.tv_usec) / 1000L;
        long remain = timeout_ms - elapsed;
        if (remain <= 0) {
            errno = ETIMEDOUT;
            return -1;
        }

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);

        struct timeval tv;
        tv.tv_sec = remain / 1000;
        tv.tv_usec = (remain % 1000) * 1000;

        int rc = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (rc < 0) return -1;
        if (rc == 0) {
            errno = ETIMEDOUT;
            return -1;
        }

        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n < (ssize_t)(sizeof(struct iphdr) + sizeof(sdhcp_udp_header_t))) continue;

        struct iphdr *ip = (struct iphdr *)buf;
        if (ip->version != 4 || ip->ihl < 5 || ip->protocol != IPPROTO_UDP) continue;

        size_t ip_hlen = ip->ihl * 4;
        if ((size_t)n < ip_hlen + sizeof(sdhcp_udp_header_t)) continue;

        sdhcp_udp_header_t *udp = (sdhcp_udp_header_t *)(buf + ip_hlen);
        if (ntohs(udp->src) != DHCP_SERVER_PORT || ntohs(udp->dst) != DHCP_CLIENT_PORT) continue;

        uint16_t udp_len = ntohs(udp->len);
        if (udp_len < sizeof(sdhcp_udp_header_t)) continue;

        size_t dhcp_len = udp_len - sizeof(sdhcp_udp_header_t);
        if (dhcp_len < offsetof(dhcp_packet_t, options)) continue;
        if (dhcp_len > sizeof(*out)) continue;
        if ((size_t)n < ip_hlen + sizeof(sdhcp_udp_header_t) + dhcp_len) continue;

        const dhcp_packet_t *d = (const dhcp_packet_t *)(buf + ip_hlen + sizeof(sdhcp_udp_header_t));

        if (d->op != DHCP_OP_BOOTREPLY) continue;
        if (d->htype != DHCP_HTYPE_ETHERNET || d->hlen != DHCP_HLEN_ETHERNET) continue;
        if (ntohl(d->xid) != xid) continue;
        if (memcmp(d->chaddr, mac, 6) != 0) continue;

        lease->yiaddr.s_addr = d->yiaddr;

        if (parse_options(d, dhcp_len, msg_type, lease) < 0) continue;

        memcpy(out, d, dhcp_len);
        *out_len = dhcp_len;
        return 0;
    }
}

int sdhcp_acquire(const char *ifname, int timeout_ms, int retries, sdhcp_lease_t *lease)
{
    if (!ifname || !lease || timeout_ms <= 0 || retries <= 0) {
        errno = EINVAL;
        return -1;
    }

    memset(lease, 0, sizeof(*lease));

    uint8_t mac[6];
    if (get_mac(ifname, mac) < 0) return -1;

    int ifindex;
    int fd = open_sock(ifname, &ifindex);
    if (fd < 0) return -1;

    uint32_t xid = make_xid();
    dhcp_packet_t tx, rx;
    uint8_t buf[PKT_BUF_SIZE];
    size_t tx_len, rx_len;
    uint8_t msg_type;

    for (int attempt = 0; attempt < retries; attempt++) {
        size_t dhcp_len = build_discover(&tx, xid, mac);
        if (build_ip_udp(buf, sizeof(buf), &tx, dhcp_len, &tx_len) < 0) break;
        if (send_buf(fd, ifindex, buf, tx_len) < 0) continue;

        if (recv_dhcp(fd, xid, mac, timeout_ms, &rx, &rx_len, &msg_type, lease) < 0) {
            continue;
        }
        if (msg_type != DHCPOFFER) continue;
        if (!rx.yiaddr || !lease->server_identifier) continue;

        dhcp_len = build_request(&tx, xid, mac, rx.yiaddr, lease->server_identifier);
        if (build_ip_udp(buf, sizeof(buf), &tx, dhcp_len, &tx_len) < 0) break;
        if (send_buf(fd, ifindex, buf, tx_len) < 0) continue;

        if (recv_dhcp(fd, xid, mac, timeout_ms, &rx, &rx_len, &msg_type, lease) < 0) {
            continue;
        }

        if (msg_type == DHCPNAK) {
            errno = EPROTO;
            close(fd);
            return -1;
        }

        if (msg_type == DHCPACK) {
            close(fd);
            return 0;
        }
    }

    close(fd);
    errno = ETIMEDOUT;
    return -1;
}
