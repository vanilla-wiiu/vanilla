#ifndef VANILLA_PI_NFC_NTAG_UTIL_H
#define VANILLA_PI_NFC_NTAG_UTIL_H

#include <stddef.h>
#include <stdint.h>

#define NTAG_PAGE_SIZE 0x4

void ntag_prepare_read_cmd(uint8_t addr, uint8_t *cmd_buf, size_t *cmd_sz, size_t *rsp_sz);

void ntag_prepare_write_cmd(uint8_t addr, const uint8_t data[NTAG_PAGE_SIZE], uint8_t *cmd_buf, size_t *cmd_sz,
                            size_t *rsp_sz);

void ntag_prepare_get_version_cmd(uint8_t *cmd_buf, size_t *cmd_sz, size_t *rsp_sz);

void ntag_prepare_read_sig_cmd(uint8_t *cmd_buf, size_t *cmd_sz, size_t *rsp_sz);

void ntag_prepare_pwd_auth_cmd(const uint8_t pwd[4], uint8_t *cmd_buf, size_t *cmd_sz, size_t *rsp_sz);

void ntag_prepare_fast_read_cmd(uint8_t start, uint8_t end, uint8_t *cmd_buf, size_t *cmd_sz, size_t *rsp_sz);

#endif // VANILLA_PI_NFC_NTAG_UTIL_H
