#include "wiiu_wowl.h"

#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/nl80211.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netlink/attr.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>
#include <netlink/msg.h>
#include <netlink/netlink.h>
#include <openssl/evp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "wpa.h"

#define WIIU_WOWL_DA_LEN 6
#define WIIU_WOWL_KEY_LEN 16
#define WIIU_WOWL_PLAINTEXT_LEN 25
#define WIIU_WOWL_CCMP_HEADER_LEN 8
#define WIIU_WOWL_CCMP_MIC_LEN 8
#define WIIU_WOWL_80211_QOS_HDR_LEN 26
#define WIIU_WOWL_RADIOTAP_LEGACY_LEN 12
#define WIIU_WOWL_RADIOTAP_MCS_LEN 13
#define WIIU_WOWL_RADIOTAP_MAX_LEN WIIU_WOWL_RADIOTAP_MCS_LEN
#define WIIU_WOWL_RADIOTAP_RATE_PRESENT 0x00000004
#define WIIU_WOWL_RADIOTAP_TX_FLAGS_PRESENT 0x00008000
#define WIIU_WOWL_RADIOTAP_MCS_PRESENT 0x00080000
#define WIIU_WOWL_RADIOTAP_TX_FLAGS_NOACK 0x0008
#define WIIU_WOWL_RADIOTAP_RATE_6MBPS 12
#define WIIU_WOWL_RADIOTAP_MCS_HAVE_BW 0x01
#define WIIU_WOWL_RADIOTAP_MCS_HAVE_MCS 0x02
#define WIIU_WOWL_RADIOTAP_MCS_HAVE_GI 0x04
#define WIIU_WOWL_RADIOTAP_MCS_KNOWN \
    (WIIU_WOWL_RADIOTAP_MCS_HAVE_BW \
        | WIIU_WOWL_RADIOTAP_MCS_HAVE_MCS \
        | WIIU_WOWL_RADIOTAP_MCS_HAVE_GI)
#define WIIU_WOWL_TX_COUNT 352
#define WIIU_WOWL_TX_MCS0_COUNT 3
#define WIIU_WOWL_TX_GAP_US 250
#define WIIU_WOWL_TX_DRAIN_US 2000
#define WIIU_WOWL_MONITOR_SETTLE_US 100000
#define WIIU_WOWL_SET_FREQ_RETRIES 6
#define WIIU_WOWL_SET_FREQ_PREFERRED_RETRIES 2
#define WIIU_WOWL_SET_FREQ_RETRY_BASE_US 20000
#define WIIU_WOWL_SET_FREQ_RETRY_MAX_US 200000
#define WIIU_WOWL_TX_PER_SEQ 16
#define WIIU_WOWL_QOS_TID 7
#define WIIU_WOWL_DURATION 0x003c
#define WIIU_WOWL_SEQ_START 0x02c0
#define WIIU_WOWL_CCMP_PN 1
#define WIIU_WOWL_ETH_HEADER_LEN 14
#define WIIU_WOWL_NET_PATTERN_OFFSET 15
#define WIIU_WOWL_NET_PATTERN_PLAINTEXT_OFFSET \
    (WIIU_WOWL_NET_PATTERN_OFFSET - WIIU_WOWL_ETH_HEADER_LEN)
#define WIIU_WOWL_PAIRING_SSID_LENGTH 16
#define WIIU_WOWL_COUNTRY_LEN 2

#ifndef ARPHRD_IEEE80211
#define ARPHRD_IEEE80211 801
#endif
#ifndef ARPHRD_IEEE80211_RADIOTAP
#define ARPHRD_IEEE80211_RADIOTAP 803
#endif

enum WowlRadiotapMode
{
    WIIU_WOWL_RADIOTAP_NONE,
    WIIU_WOWL_RADIOTAP_LEGACY,
    WIIU_WOWL_RADIOTAP_MCS0
};

static const uint32_t wiiu_wowl_sweep_frequencies[] = {
    5180, 5200, 5220, 5240, 5260, 5280, 5300, 5320,
    5500, 5520, 5540, 5560, 5580, 5600, 5620, 5640,
    5660, 5680, 5700, 5720, 5745, 5765, 5785, 5805,
    5825
};

static void put_le16(unsigned char *out, uint16_t value)
{
    out[0] = value & 0xff;
    out[1] = value >> 8;
}

static void format_mac(const unsigned char mac[WIIU_WOWL_DA_LEN], char *out, size_t out_size)
{
    snprintf(out, out_size, "%02x:%02x:%02x:%02x:%02x:%02x",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void region_country_code(uint8_t region, unsigned char country[WIIU_WOWL_COUNTRY_LEN])
{
    switch (region) {
    case VANILLA_REGION_JAPAN:
        country[0] = 'J';
        country[1] = 'P';
        break;
    case VANILLA_REGION_EUROPE:
    case VANILLA_REGION_AUSTRALIA:
        country[0] = 'E';
        country[1] = 'U';
        break;
    case VANILLA_REGION_AMERICA:
    default:
        country[0] = 'Q';
        country[1] = '2';
        break;
    }
}

static void derive_sta1_wowl_key(
    unsigned char key[WIIU_WOWL_KEY_LEN],
    uint8_t region,
    uint8_t seed,
    const unsigned char wake_token[WIIU_WOWL_DA_LEN],
    const unsigned char source_mac[WIIU_WOWL_DA_LEN])
{
    unsigned char country[WIIU_WOWL_COUNTRY_LEN];
    region_country_code(region, country);

    unsigned char raw[WIIU_WOWL_KEY_LEN];
    memset(raw, 0, sizeof(raw));
    raw[0] = WIIU_WOWL_PAIRING_SSID_LENGTH;
    raw[1] = country[0];
    raw[2] = country[1];
    raw[3] = WIIU_WOWL_COUNTRY_LEN;
    memcpy(raw + 4, wake_token, WIIU_WOWL_DA_LEN);
    memcpy(raw + 10, source_mac, WIIU_WOWL_DA_LEN);

    uint8_t prev = seed;
    for (int i = WIIU_WOWL_KEY_LEN - 1; i >= 0; i--) {
        prev ^= raw[i];
        key[i] = prev;
    }
}

static uint8_t seed_from_psk(const vanilla_psk_t *psk)
{
    return psk->psk[sizeof(psk->psk) - 1] & 0x0f;
}

static int get_interface_info(const char *ifname, unsigned char mac[WIIU_WOWL_DA_LEN], int *arphrd)
{
    int skt = socket(AF_INET, SOCK_DGRAM, 0);
    if (skt < 0) {
        nlprint("Wii U WOWL: failed to open ioctl socket: %i", errno);
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);

    if (ioctl(skt, SIOCGIFHWADDR, &ifr) < 0) {
        nlprint("Wii U WOWL: failed to read %s hardware address: %i", ifname, errno);
        close(skt);
        return -1;
    }

    memcpy(mac, ifr.ifr_hwaddr.sa_data, WIIU_WOWL_DA_LEN);
    *arphrd = ifr.ifr_hwaddr.sa_family;
    close(skt);
    return 0;
}

static int get_interface_flags(const char *ifname, short *flags)
{
    int skt = socket(AF_INET, SOCK_DGRAM, 0);
    if (skt < 0) {
        nlprint("Wii U WOWL: failed to open ioctl socket: %i", errno);
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);

    if (ioctl(skt, SIOCGIFFLAGS, &ifr) < 0) {
        nlprint("Wii U WOWL: failed to read %s flags: %i", ifname, errno);
        close(skt);
        return -1;
    }

    *flags = ifr.ifr_flags;
    close(skt);
    return 0;
}

static int set_interface_up_state(const char *ifname, int up)
{
    short flags = 0;
    if (get_interface_flags(ifname, &flags) < 0) {
        return -1;
    }

    int skt = socket(AF_INET, SOCK_DGRAM, 0);
    if (skt < 0) {
        nlprint("Wii U WOWL: failed to open ioctl socket: %i", errno);
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
    ifr.ifr_flags = up ? (flags | IFF_UP) : (flags & ~IFF_UP);

    if (ioctl(skt, SIOCSIFFLAGS, &ifr) < 0) {
        nlprint("Wii U WOWL: failed to bring %s %s: %i", ifname, up ? "up" : "down", errno);
        close(skt);
        return -1;
    }

    close(skt);
    return 0;
}

static int set_interface_up(const char *ifname)
{
    return set_interface_up_state(ifname, 1);
}

static int set_interface_down(const char *ifname)
{
    return set_interface_up_state(ifname, 0);
}

typedef int (*Nl80211AddAttrs)(struct nl_msg *msg, void *data);

static int nl80211_send(
    const char *ifname,
    enum nl80211_commands command,
    const char *description,
    Nl80211AddAttrs add_attrs,
    void *data)
{
    const char *target = ifname ? ifname : "global";
    unsigned int ifindex = 0;
    if (ifname) {
        ifindex = if_nametoindex(ifname);
        if (!ifindex) {
            nlprint("Wii U WOWL: failed to resolve interface index for %s: %i",
                ifname, errno);
            return -1;
        }
    }

    struct nl_sock *skt = nl_socket_alloc();
    if (!skt) {
        nlprint("Wii U WOWL: failed to allocate netlink socket");
        return -1;
    }

    int ret = genl_connect(skt);
    if (ret < 0) {
        nlprint("Wii U WOWL: failed to connect generic netlink socket: %s",
            nl_geterror(ret));
        nl_socket_free(skt);
        return -1;
    }

    int nl80211_id = genl_ctrl_resolve(skt, "nl80211");
    if (nl80211_id < 0) {
        nlprint("Wii U WOWL: failed to resolve nl80211 family: %s",
            nl_geterror(nl80211_id));
        nl_socket_free(skt);
        return -1;
    }

    struct nl_msg *msg = nlmsg_alloc();
    if (!msg) {
        nlprint("Wii U WOWL: failed to allocate nl80211 message");
        nl_socket_free(skt);
        return -1;
    }

    if (!genlmsg_put(msg, 0, 0, nl80211_id, 0, 0, command, 0)
        || (ifname && nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifindex) < 0)
        || add_attrs(msg, data) < 0) {
        nlprint("Wii U WOWL: failed to build nl80211 %s request for %s",
            description, target);
        nlmsg_free(msg);
        nl_socket_free(skt);
        return -1;
    }

    ret = nl_send_auto_complete(skt, msg);
    nlmsg_free(msg);
    if (ret < 0) {
        nlprint("Wii U WOWL: failed to send nl80211 %s request for %s: %s",
            description, target, nl_geterror(ret));
        nl_socket_free(skt);
        return ret;
    }

    ret = nl_wait_for_ack(skt);
    nl_socket_free(skt);
    if (ret < 0) {
        nlprint("Wii U WOWL: nl80211 %s request failed for %s: %s",
            description, target, nl_geterror(ret));
        return ret;
    }

    return 0;
}

static int nl80211_error_is_busy(int ret)
{
    const char *error = ret < 0 ? nl_geterror(ret) : NULL;
    return ret == -EBUSY || (error && !strcmp(error, "Object busy"));
}

static int nl80211_add_interface_type(struct nl_msg *msg, void *data)
{
    const enum nl80211_iftype *iftype = data;
    return nla_put_u32(msg, NL80211_ATTR_IFTYPE, *iftype);
}

static int nl80211_add_frequency(struct nl_msg *msg, void *data)
{
    const uint32_t *frequency = data;
    return nla_put_u32(msg, NL80211_ATTR_WIPHY_FREQ, *frequency);
}

static int nl80211_add_reg_alpha2(struct nl_msg *msg, void *data)
{
    const char *alpha2 = data;
    return nla_put(msg, NL80211_ATTR_REG_ALPHA2, 2, alpha2);
}

static int nl80211_set_interface_type(
    const char *ifname,
    enum nl80211_iftype iftype,
    const char *name)
{
    if (nl80211_send(ifname, NL80211_CMD_SET_INTERFACE, "set interface type",
            nl80211_add_interface_type, &iftype) < 0) {
        return -1;
    }

    nlprint("Wii U WOWL: nl80211 set %s type %s", ifname, name);
    return 0;
}

static int nl80211_set_frequency_retry(
    const char *ifname,
    uint32_t frequency,
    int retries)
{
    int ret = 0;

    for (int attempt = 0; attempt <= retries; attempt++) {
        ret = nl80211_send(ifname, NL80211_CMD_SET_WIPHY, "set frequency",
            nl80211_add_frequency, &frequency);
        if (ret == 0) {
            nlprint("Wii U WOWL: nl80211 tuned %s to %u MHz", ifname, frequency);
            return 0;
        }

        if (!nl80211_error_is_busy(ret) || attempt == retries
            || is_interrupted()) {
            break;
        }

        unsigned int delay = WIIU_WOWL_SET_FREQ_RETRY_BASE_US << attempt;
        if (delay > WIIU_WOWL_SET_FREQ_RETRY_MAX_US) {
            delay = WIIU_WOWL_SET_FREQ_RETRY_MAX_US;
        }

        nlprint("Wii U WOWL: %s busy while tuning to %u MHz, retrying in %u us (%d/%d)",
            ifname, frequency, delay, attempt + 1, retries);
        usleep(delay);
    }

    if (nl80211_error_is_busy(ret)) {
        nlprint("Wii U WOWL: failed to tune %s to %u MHz after busy retries",
            ifname, frequency);
    }

    return -1;
}

static void build_wowl_plaintext(
    unsigned char plaintext[WIIU_WOWL_PLAINTEXT_LEN],
    const unsigned char dst[WIIU_WOWL_DA_LEN])
{
    memset(plaintext, 0, WIIU_WOWL_PLAINTEXT_LEN);
    memcpy(plaintext + WIIU_WOWL_NET_PATTERN_PLAINTEXT_OFFSET,
        dst, WIIU_WOWL_DA_LEN);
}

static void build_qos_data_header(
    unsigned char hdr[WIIU_WOWL_80211_QOS_HDR_LEN],
    const unsigned char dst[WIIU_WOWL_DA_LEN],
    const unsigned char src[WIIU_WOWL_DA_LEN],
    uint16_t seq_control,
    int retry)
{
    uint16_t fc = 0x4188; // QoS Data, ToDS, Protected.
    if (retry) {
        fc |= 0x0800;
    }

    memset(hdr, 0, WIIU_WOWL_80211_QOS_HDR_LEN);
    put_le16(hdr + 0, fc);
    put_le16(hdr + 2, WIIU_WOWL_DURATION);
    memcpy(hdr + 4, dst, WIIU_WOWL_DA_LEN);   // RA/BSSID
    memcpy(hdr + 10, src, WIIU_WOWL_DA_LEN);  // TA/SA
    memcpy(hdr + 16, dst, WIIU_WOWL_DA_LEN);  // DA
    put_le16(hdr + 22, seq_control);
    put_le16(hdr + 24, WIIU_WOWL_QOS_TID);
}

static void build_ccmp_header(unsigned char ccmp[WIIU_WOWL_CCMP_HEADER_LEN])
{
    memset(ccmp, 0, WIIU_WOWL_CCMP_HEADER_LEN);
    ccmp[0] = WIIU_WOWL_CCMP_PN & 0xff;
    ccmp[1] = (WIIU_WOWL_CCMP_PN >> 8) & 0xff;
    ccmp[3] = 0x20; // ExtIV set, key index 0.
}

static size_t radiotap_len_for_mode(enum WowlRadiotapMode mode)
{
    switch (mode) {
    case WIIU_WOWL_RADIOTAP_LEGACY:
        return WIIU_WOWL_RADIOTAP_LEGACY_LEN;
    case WIIU_WOWL_RADIOTAP_MCS0:
        return WIIU_WOWL_RADIOTAP_MCS_LEN;
    case WIIU_WOWL_RADIOTAP_NONE:
    default:
        return 0;
    }
}

static size_t build_radiotap_header(
    unsigned char *frame,
    size_t frame_size,
    enum WowlRadiotapMode mode)
{
    size_t radiotap_len = radiotap_len_for_mode(mode);
    if (!radiotap_len) {
        return 0;
    }
    if (frame_size < radiotap_len) {
        return 0;
    }

    memset(frame, 0, radiotap_len);
    put_le16(frame + 2, (uint16_t) radiotap_len);

    switch (mode) {
    case WIIU_WOWL_RADIOTAP_LEGACY: {
        uint32_t present = WIIU_WOWL_RADIOTAP_RATE_PRESENT
            | WIIU_WOWL_RADIOTAP_TX_FLAGS_PRESENT;
        frame[4] = present & 0xff;
        frame[5] = (present >> 8) & 0xff;
        frame[6] = (present >> 16) & 0xff;
        frame[7] = (present >> 24) & 0xff;
        frame[8] = WIIU_WOWL_RADIOTAP_RATE_6MBPS;
        put_le16(frame + 10, WIIU_WOWL_RADIOTAP_TX_FLAGS_NOACK);
        break;
    }
    case WIIU_WOWL_RADIOTAP_MCS0: {
        uint32_t present = WIIU_WOWL_RADIOTAP_TX_FLAGS_PRESENT
            | WIIU_WOWL_RADIOTAP_MCS_PRESENT;
        frame[4] = present & 0xff;
        frame[5] = (present >> 8) & 0xff;
        frame[6] = (present >> 16) & 0xff;
        frame[7] = (present >> 24) & 0xff;
        put_le16(frame + 8, WIIU_WOWL_RADIOTAP_TX_FLAGS_NOACK);
        frame[10] = WIIU_WOWL_RADIOTAP_MCS_KNOWN;
        frame[11] = 0; // 20 MHz, long GI.
        frame[12] = 0; // MCS index 0.
        break;
    }
    case WIIU_WOWL_RADIOTAP_NONE:
    default:
        break;
    }

    return radiotap_len;
}

static void build_ccmp_aad(
    unsigned char aad[24],
    const unsigned char hdr[WIIU_WOWL_80211_QOS_HDR_LEN])
{
    uint16_t fc = hdr[0] | (hdr[1] << 8);
    uint16_t seq = hdr[22] | (hdr[23] << 8);
    uint16_t qos = hdr[24] | (hdr[25] << 8);

    fc &= (uint16_t) ~(0x0800 | 0x1000 | 0x2000);
    fc &= (uint16_t) ~0x0070;
    fc |= 0x4000;
    fc &= (uint16_t) ~0x8000;
    seq &= 0x000f;
    qos &= 0x000f;

    put_le16(aad + 0, fc);
    memcpy(aad + 2, hdr + 4, 18);
    put_le16(aad + 20, seq);
    put_le16(aad + 22, qos);
}

static void build_ccmp_nonce(
    unsigned char nonce[13],
    const unsigned char src[WIIU_WOWL_DA_LEN])
{
    memset(nonce, 0, 13);
    nonce[0] = WIIU_WOWL_QOS_TID;
    memcpy(nonce + 1, src, WIIU_WOWL_DA_LEN);
    nonce[12] = WIIU_WOWL_CCMP_PN & 0xff;
}

static int encrypt_ccmp(
    const unsigned char key[WIIU_WOWL_KEY_LEN],
    const unsigned char nonce[13],
    const unsigned char *aad,
    size_t aad_len,
    const unsigned char *plaintext,
    size_t plaintext_len,
    unsigned char *ciphertext,
    unsigned char mic[WIIU_WOWL_CCMP_MIC_LEN])
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        nlprint("Wii U WOWL: failed to allocate OpenSSL cipher context");
        return -1;
    }

    int ok = 0;
    int out_len = 0;
    int tmp_len = 0;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_ccm(), NULL, NULL, NULL) != 1) {
        goto exit;
    }
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_SET_IVLEN, 13, NULL) != 1) {
        goto exit;
    }
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_SET_TAG, WIIU_WOWL_CCMP_MIC_LEN, NULL) != 1) {
        goto exit;
    }
    if (EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) != 1) {
        goto exit;
    }
    if (EVP_EncryptUpdate(ctx, NULL, &out_len, NULL, (int) plaintext_len) != 1) {
        goto exit;
    }
    if (EVP_EncryptUpdate(ctx, NULL, &out_len, aad, (int) aad_len) != 1) {
        goto exit;
    }
    if (EVP_EncryptUpdate(ctx, ciphertext, &out_len, plaintext, (int) plaintext_len) != 1) {
        goto exit;
    }
    if (EVP_EncryptFinal_ex(ctx, ciphertext + out_len, &tmp_len) != 1) {
        goto exit;
    }
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_GET_TAG, WIIU_WOWL_CCMP_MIC_LEN, mic) != 1) {
        goto exit;
    }

    ok = 1;

exit:
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) {
        nlprint("Wii U WOWL: failed to encrypt CCMP wake payload");
        return -1;
    }
    return 0;
}

static size_t build_wake_frame(
    unsigned char *frame,
    size_t frame_size,
    enum WowlRadiotapMode radiotap_mode,
    const unsigned char key[WIIU_WOWL_KEY_LEN],
    const unsigned char dst[WIIU_WOWL_DA_LEN],
    const unsigned char src[WIIU_WOWL_DA_LEN],
    int frame_index)
{
    const size_t radiotap_len = radiotap_len_for_mode(radiotap_mode);
    const size_t frame_len = radiotap_len
        + WIIU_WOWL_80211_QOS_HDR_LEN
        + WIIU_WOWL_CCMP_HEADER_LEN
        + WIIU_WOWL_PLAINTEXT_LEN
        + WIIU_WOWL_CCMP_MIC_LEN;

    if (frame_size < frame_len) {
        return 0;
    }

    unsigned char hdr[WIIU_WOWL_80211_QOS_HDR_LEN];
    unsigned char aad[24];
    unsigned char nonce[13];
    unsigned char plaintext[WIIU_WOWL_PLAINTEXT_LEN];
    unsigned char ccmp[WIIU_WOWL_CCMP_HEADER_LEN];
    unsigned char ciphertext[WIIU_WOWL_PLAINTEXT_LEN];
    unsigned char mic[WIIU_WOWL_CCMP_MIC_LEN];
    uint16_t seq_control = WIIU_WOWL_SEQ_START
        + (uint16_t) ((frame_index / WIIU_WOWL_TX_PER_SEQ) << 4);
    int retry = (frame_index % WIIU_WOWL_TX_PER_SEQ) != 0;

    build_qos_data_header(hdr, dst, src, seq_control, retry);
    build_ccmp_aad(aad, hdr);
    build_ccmp_nonce(nonce, src);
    build_wowl_plaintext(plaintext, dst);
    build_ccmp_header(ccmp);

    if (encrypt_ccmp(key, nonce, aad, sizeof(aad),
            plaintext, sizeof(plaintext), ciphertext, mic) < 0) {
        return 0;
    }

    size_t offset = 0;
    if (radiotap_len) {
        offset += build_radiotap_header(frame, frame_size, radiotap_mode);
    }

    memcpy(frame + offset, hdr, sizeof(hdr));
    offset += sizeof(hdr);
    memcpy(frame + offset, ccmp, sizeof(ccmp));
    offset += sizeof(ccmp);
    memcpy(frame + offset, ciphertext, sizeof(ciphertext));
    offset += sizeof(ciphertext);
    memcpy(frame + offset, mic, sizeof(mic));
    offset += sizeof(mic);

    return offset;
}

static int inject_wake_frames(
    const char *ifname,
    int arphrd,
    const unsigned char key[WIIU_WOWL_KEY_LEN],
    const unsigned char dst[WIIU_WOWL_DA_LEN],
    const unsigned char src[WIIU_WOWL_DA_LEN])
{
    enum WowlRadiotapMode radiotap_mode = WIIU_WOWL_RADIOTAP_NONE;
    if (arphrd == ARPHRD_IEEE80211_RADIOTAP) {
        radiotap_mode = WIIU_WOWL_RADIOTAP_LEGACY;
    } else if (arphrd != ARPHRD_IEEE80211) {
        nlprint("Wii U WOWL: %s is not a monitor/raw 802.11 interface (ARPHRD=%d); skipping wake packet",
            ifname, arphrd);
        return -1;
    }

    unsigned int ifindex = if_nametoindex(ifname);
    if (!ifindex) {
        nlprint("Wii U WOWL: failed to resolve interface index for %s: %i", ifname, errno);
        return -1;
    }

    char dst_str[18];
    char src_str[18];
    format_mac(dst, dst_str, sizeof(dst_str));
    format_mac(src, src_str, sizeof(src_str));
    nlprint("Wii U WOWL: injecting Broadcom net-pattern wake frame on %s: dst=%s src=%s pn=%u mcs0=%u legacy=%u",
        ifname, dst_str, src_str, WIIU_WOWL_CCMP_PN,
        radiotap_mode == WIIU_WOWL_RADIOTAP_LEGACY ? WIIU_WOWL_TX_MCS0_COUNT : 0,
        WIIU_WOWL_TX_COUNT);

    int skt = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (skt < 0) {
        nlprint("Wii U WOWL: failed to open packet socket: %i", errno);
        return -1;
    }

    struct sockaddr_ll addr;
    memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_ALL);
    addr.sll_ifindex = (int) ifindex;
    addr.sll_halen = WIIU_WOWL_DA_LEN;
    memcpy(addr.sll_addr, dst, WIIU_WOWL_DA_LEN);

    unsigned char frame[WIIU_WOWL_RADIOTAP_MAX_LEN
        + WIIU_WOWL_80211_QOS_HDR_LEN
        + WIIU_WOWL_CCMP_HEADER_LEN
        + WIIU_WOWL_PLAINTEXT_LEN
        + WIIU_WOWL_CCMP_MIC_LEN];

    int sent = 0;
    int last_errno = 0;
    if (radiotap_mode == WIIU_WOWL_RADIOTAP_LEGACY) {
        for (int i = 0; i < WIIU_WOWL_TX_MCS0_COUNT && !is_interrupted(); i++) {
            size_t frame_len = build_wake_frame(frame, sizeof(frame), WIIU_WOWL_RADIOTAP_MCS0,
                key, dst, src, i);

            if (!frame_len) {
                close(skt);
                return -1;
            }

            ssize_t written = sendto(skt, frame, frame_len, 0,
                (const struct sockaddr *) &addr, sizeof(addr));
            if (written == (ssize_t) frame_len) {
                sent++;
            } else {
                last_errno = errno;
            }

            usleep(WIIU_WOWL_TX_GAP_US);
        }
    }

    for (int i = 0; i < WIIU_WOWL_TX_COUNT && !is_interrupted(); i++) {
        int frame_index = i
            + (radiotap_mode == WIIU_WOWL_RADIOTAP_LEGACY ? WIIU_WOWL_TX_MCS0_COUNT : 0);
        size_t frame_len = build_wake_frame(frame, sizeof(frame), radiotap_mode,
            key, dst, src, frame_index);

        if (!frame_len) {
            close(skt);
            return -1;
        }

        ssize_t written = sendto(skt, frame, frame_len, 0,
            (const struct sockaddr *) &addr, sizeof(addr));
        if (written == (ssize_t) frame_len) {
            sent++;
        } else {
            last_errno = errno;
        }

        usleep(WIIU_WOWL_TX_GAP_US);
    }

    close(skt);

    if (!sent) {
        nlprint("Wii U WOWL: failed to inject wake frames: %i", last_errno);
        return -1;
    }

    unsigned int expected = WIIU_WOWL_TX_COUNT
        + (radiotap_mode == WIIU_WOWL_RADIOTAP_LEGACY ? WIIU_WOWL_TX_MCS0_COUNT : 0);
    if (sent != (int) expected) {
        nlprint("Wii U WOWL: injected %d/%u wake frames, last error: %i",
            sent, expected, last_errno);
    } else {
        nlprint("Wii U WOWL: injected %d wake frames", sent);
    }

    return 0;
}

static int tune_and_inject_wake(
    const char *ifname,
    int arphrd,
    uint32_t frequency,
    int frequency_retries,
    const unsigned char key[WIIU_WOWL_KEY_LEN],
    const unsigned char dst[WIIU_WOWL_DA_LEN],
    const unsigned char src[WIIU_WOWL_DA_LEN])
{
    if (nl80211_set_frequency_retry(ifname, frequency, frequency_retries) < 0) {
        return -1;
    }

    int ret = inject_wake_frames(ifname, arphrd, key, dst, src);
    usleep(WIIU_WOWL_TX_DRAIN_US);
    return ret;
}

static int sweep_wake_frequencies(
    const char *ifname,
    int arphrd,
    uint32_t preferred_frequency,
    const unsigned char key[WIIU_WOWL_KEY_LEN],
    const unsigned char dst[WIIU_WOWL_DA_LEN],
    const unsigned char src[WIIU_WOWL_DA_LEN])
{
    int sent_any = 0;
    int tried_preferred = 0;
    size_t frequency_count = sizeof(wiiu_wowl_sweep_frequencies)
        / sizeof(wiiu_wowl_sweep_frequencies[0]);

    if (preferred_frequency) {
        nlprint("Wii U WOWL: sweeping %zu 5GHz wake channels, preferred %u MHz first",
            frequency_count, preferred_frequency);
    } else {
        nlprint("Wii U WOWL: sweeping %zu 5GHz wake channels", frequency_count);
    }

    if (preferred_frequency) {
        tried_preferred = 1;
        if (tune_and_inject_wake(ifname, arphrd, preferred_frequency,
                WIIU_WOWL_SET_FREQ_PREFERRED_RETRIES, key, dst, src) == 0) {
            sent_any = 1;
        }
    }

    for (size_t i = 0; i < frequency_count && !is_interrupted(); i++) {
        uint32_t frequency = wiiu_wowl_sweep_frequencies[i];
        if (tried_preferred && frequency == preferred_frequency) {
            continue;
        }

        if (tune_and_inject_wake(ifname, arphrd, frequency,
                WIIU_WOWL_SET_FREQ_RETRIES, key, dst, src) == 0) {
            sent_any = 1;
        }
    }

    if (!sent_any) {
        nlprint("Wii U WOWL: failed to inject wake frames on any swept channel");
        return -1;
    }

    return 0;
}

int wiiu_wowl_try_wake(const char *ifname, const vanilla_connection_t *connection)
{
    if (!ifname || !connection) {
        nlprint("Wii U WOWL: missing interface or connection");
        return -1;
    }

    unsigned char iface_mac[WIIU_WOWL_DA_LEN];
    int arphrd = 0;
    if (get_interface_info(ifname, iface_mac, &arphrd) < 0) {
        return -1;
    }

    const char *tx_ifname = ifname;
    int tx_arphrd = arphrd;
    int restore_station_type = 0;
    int restore_base_up = 0;

    if (arphrd == ARPHRD_ETHER) {
        short base_flags = 0;
        if (get_interface_flags(ifname, &base_flags) == 0 && (base_flags & IFF_UP)) {
            if (set_interface_down(ifname) < 0) {
                return -1;
            }
            restore_base_up = 1;
            nlprint("Wii U WOWL: temporarily brought %s down for channel control", ifname);
        }

        if (nl80211_set_interface_type(ifname, NL80211_IFTYPE_MONITOR, "monitor") < 0) {
            if (restore_base_up) {
                set_interface_up(ifname);
            }
            return -1;
        }

        restore_station_type = 1;
        tx_arphrd = ARPHRD_IEEE80211_RADIOTAP;

        unsigned char type_check_mac[WIIU_WOWL_DA_LEN];
        if (get_interface_info(tx_ifname, type_check_mac, &tx_arphrd) < 0) {
            nl80211_set_interface_type(ifname, NL80211_IFTYPE_STATION, "station");
            if (restore_base_up) {
                set_interface_up(ifname);
            }
            return -1;
        }

        nlprint("Wii U WOWL: temporarily set %s to monitor mode", ifname);
        if (set_interface_up(ifname) < 0) {
            nl80211_set_interface_type(ifname, NL80211_IFTYPE_STATION, "station");
            if (restore_base_up) {
                set_interface_up(ifname);
            }
            return -1;
        }
        usleep(WIIU_WOWL_MONITOR_SETTLE_US);
    } else {
        short tx_flags = 0;
        if (get_interface_flags(tx_ifname, &tx_flags) == 0 && (tx_flags & IFF_UP)) {
            if (set_interface_down(tx_ifname) < 0) {
                return -1;
            }
            nlprint("Wii U WOWL: temporarily brought %s down for channel control", tx_ifname);
        }

        if (set_interface_up(tx_ifname) < 0) {
            return -1;
        }
        usleep(WIIU_WOWL_MONITOR_SETTLE_US);
    }

    unsigned char country[WIIU_WOWL_COUNTRY_LEN];
    region_country_code(connection->region, country);

    unsigned char wowl_key[WIIU_WOWL_KEY_LEN];
    uint8_t seed = seed_from_psk(&connection->psk);
    nlprint("Wii U WOWL: My seed is: %x", seed);
    derive_sta1_wowl_key(wowl_key, connection->region, seed,
        connection->bssid.bssid, iface_mac);
    nlprint("Wii U WOWL: using region %u country %c%c seed 0x%x for key derivation",
        connection->region, country[0], country[1], seed);

    int ret = sweep_wake_frequencies(tx_ifname, tx_arphrd, connection->wifi_frequency,
        wowl_key, connection->bssid.bssid, iface_mac);

    if (restore_station_type) {
        set_interface_down(ifname);
        nl80211_set_interface_type(ifname, NL80211_IFTYPE_STATION, "station");
        nlprint("Wii U WOWL: restored %s to station mode", ifname);
    }
    if (restore_base_up) {
        set_interface_up(ifname);
        nlprint("Wii U WOWL: restored %s after wake injection", ifname);
    }

    return ret;
}
