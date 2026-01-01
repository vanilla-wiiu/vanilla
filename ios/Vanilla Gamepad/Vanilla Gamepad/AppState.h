#pragma once

typedef enum {
    STATE_STARTUP,
    STATE_CONNECTION_MODE,
    STATE_ENTER_IP,
    STATE_CONNECTING,
    STATE_STREAMING
} AppState;
