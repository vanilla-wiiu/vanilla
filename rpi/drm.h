#ifndef VANILLA_PI_DRM_H
#define VANILLA_PI_DRM_H

typedef struct {
	int fd;
	int crtc;
} vanilla_drm_ctx_t;

int initialize_drm(vanilla_drm_ctx_t *ctx);
int free_drm(vanilla_drm_ctx_t *ctx);

#endif // VANILLA_PI_DRM_H