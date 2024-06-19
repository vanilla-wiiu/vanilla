#ifndef VANILLA_PIPE_H
#define VANILLA_PIPE_H

// Control codes that our pipe will receive
#define VANILLA_PIPE_IN_SYNC         0x01
#define VANILLA_PIPE_IN_CONNECT      0x02
#define VANILLA_PIPE_IN_BUTTON       0x03
#define VANILLA_PIPE_IN_TOUCH        0x04
#define VANILLA_PIPE_IN_REQ_IDR      0x05
#define VANILLA_PIPE_IN_REGION       0x06
#define VANILLA_PIPE_IN_INTERRUPT    0x1E
#define VANILLA_PIPE_IN_QUIT         0x1F

#define VANILLA_PIPE_OUT_DATA        0x20
#define VANILLA_PIPE_OUT_SYNC_STATE  0x21
#define VANILLA_PIPE_OUT_EOF         0x22

// Errors that our pipe will send
#define VANILLA_PIPE_ERR_SUCCESS     0x30
#define VANILLA_PIPE_ERR_UNK         0x31
#define VANILLA_PIPE_ERR_INVALID     0x32
#define VANILLA_PIPE_ERR_BUSY        0x33

#endif // VANILLA_PIPE_H