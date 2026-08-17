#include "ntag_util.h"

enum {
    CMD_READ = 0x30,
    CMD_WRITE = 0xA2,
    CMD_GET_VERSION = 0x60,
    CMD_READ_SIG = 0x3C,
    CMD_PWD_AUTH = 0x1B,
    CMD_FAST_READ = 0x3A,
};

void ntag_prepare_read_cmd(uint8_t addr, uint8_t *cmd_buf, size_t *cmd_sz, size_t *rsp_sz)
{
    cmd_buf[0] = CMD_READ;
    cmd_buf[1] = addr;

    *cmd_sz = 2;
    *rsp_sz = 16;
}

void ntag_prepare_write_cmd(uint8_t addr, const uint8_t data[NTAG_PAGE_SIZE], uint8_t *cmd_buf, size_t *cmd_sz,
                            size_t *rsp_sz)
{
    cmd_buf[0] = CMD_WRITE;
    cmd_buf[1] = addr;
    cmd_buf[2] = data[0];
    cmd_buf[3] = data[1];
    cmd_buf[4] = data[2];
    cmd_buf[5] = data[3];

    *cmd_sz = 6;
    *rsp_sz = 1;
}

void ntag_prepare_get_version_cmd(uint8_t *cmd_buf, size_t *cmd_sz, size_t *rsp_sz)
{
    cmd_buf[0] = CMD_GET_VERSION;

    *cmd_sz = 1;
    *rsp_sz = 8;
}

void ntag_prepare_read_sig_cmd(uint8_t *cmd_buf, size_t *cmd_sz, size_t *rsp_sz)
{
    cmd_buf[0] = CMD_READ_SIG;
    cmd_buf[1] = 0; // RFU

    *cmd_sz = 2;
    *rsp_sz = 32;
}

void ntag_prepare_pwd_auth_cmd(const uint8_t pwd[4], uint8_t *cmd_buf, size_t *cmd_sz, size_t *rsp_sz)
{
    cmd_buf[0] = CMD_PWD_AUTH;
    cmd_buf[1] = pwd[0];
    cmd_buf[2] = pwd[1];
    cmd_buf[3] = pwd[2];
    cmd_buf[4] = pwd[3];

    *cmd_sz = 5;
    *rsp_sz = 2;
}

void ntag_prepare_fast_read_cmd(uint8_t start, uint8_t end, uint8_t *cmd_buf, size_t *cmd_sz, size_t *rsp_sz)
{
    cmd_buf[0] = CMD_FAST_READ;
    cmd_buf[1] = start;
    cmd_buf[2] = end;

    *cmd_sz = 3;
    *rsp_sz = (end - start + 1) * NTAG_PAGE_SIZE;
}
