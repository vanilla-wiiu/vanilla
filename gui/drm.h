#ifndef VANILLA_PI_DRM_H
#define VANILLA_PI_DRM_H

#include <libavutil/frame.h>
#include <libavutil/hwcontext_drm.h>
#include <linux/kd.h>
#include <stdint.h>

typedef struct {
	int fd;
	uint32_t crtc;
	int crtc_index;
	uint32_t plane_id;
	int got_plane;
	uint32_t fb_id;
	int got_fb;
	uint32_t handles[AV_DRM_MAX_PLANES];
	int got_handles;
} vanilla_drm_ctx_t;

int set_tty(int mode);
int initialize_drm(vanilla_drm_ctx_t *ctx);
int free_drm(vanilla_drm_ctx_t *ctx);
int display_drm(vanilla_drm_ctx_t *ctx, AVFrame *frame);

#endif // VANILLA_PI_DRM_H
