#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Reads the next framebuffer from the backend.
 *
 * pixels: destination buffer (RGBA)
 * pitch:  bytes per row
 *
 * Returns 0 on success, < 0 on error / no frame.
 */
int Pipe_ReadFramebuffer(void *pixels, int pitch);

/*
 * Input forwarding
 */
void Pipe_SendButton(int button, int pressed);
void Pipe_SendAxis(int axis, int value);

#ifdef __cplusplus
}
#endif
