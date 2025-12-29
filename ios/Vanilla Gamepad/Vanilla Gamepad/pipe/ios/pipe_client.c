#include "pipe_client.h"
#include <string.h>

/*
 * TEMPORARY stub.
 * This allows the app to link and run.
 * We will replace this with real socket code next.
 */

int Pipe_ReadFramebuffer(void *pixels, int pitch)
{
    // Visual confirmation: moving color pattern
    static unsigned char c = 0;
    c++;

    // Fill 720p RGBA test pattern
    for (int y = 0; y < 720; y++) {
        unsigned char *row = (unsigned char *)pixels + y * pitch;
        for (int x = 0; x < 1280; x++) {
            row[x * 4 + 0] = c;       // R
            row[x * 4 + 1] = 0;       // G
            row[x * 4 + 2] = 255-c;   // B
            row[x * 4 + 3] = 255;     // A
        }
    }

    return 0;
}

void Pipe_SendButton(int button, int pressed)
{
    (void)button;
    (void)pressed;
}

void Pipe_SendAxis(int axis, int value)
{
    (void)axis;
    (void)value;
}
