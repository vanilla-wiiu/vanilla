#ifndef VANILLA_PIPE_H
#define VANILLA_PIPE_H

// Control codes that our pipe will receive
#define VANILLA_IN_SYNC     0x00
#define VANILLA_IN_CONNECT  0x01
#define VANILLA_IN_BUTTON   0x02
#define VANILLA_IN_TOUCH    0x03
#define VANILLA_IN_QUIT     0x1F

#define VANILLA_OUT_DATA    0x20

// Errors that our pipe will send
#define VANILLA_ERR_SUCCESS 0x30
#define VANILLA_ERR_UNK     0x31
#define VANILLA_ERR_INVALID 0x32
#define VANILLA_ERR_BUSY    0x33

#endif // VANILLA_PIPE_H