#include "drm.h"

#include <stdio.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

int initialize_drm(vanilla_drm_ctx_t *ctx)
{
	// Open DRM
	ctx->fd = drmOpen("vc4", 0);
	if (ctx->fd == -1) {
		return 0;
	}

	int ret = 0;

	// Find DRM output
	drmModeResPtr res = drmModeGetResources(ctx->fd);

	for (int i = 0; i < res->count_connectors; i++) {
		drmModeConnectorPtr c = drmModeGetConnector(ctx->fd, res->connectors[i]);
		if (c->encoder_id) {
			drmModeEncoderPtr enc = drmModeGetEncoder(ctx->fd, c->encoder_id);
			if (enc->crtc_id) {
				drmModeCrtcPtr crtc = drmModeGetCrtc(ctx->fd, enc->crtc_id);

				// Good! We can use this connector :)
				printf("Found output - X: %i  Y: %i  W: %i  H: %i\n", crtc->x, crtc->y, crtc->width, crtc->height);
				ctx->crtc = enc->crtc_id;

                ret = 1;

				drmModeFreeCrtc(crtc);
			}
			drmModeFreeEncoder(enc);
		}
		drmModeFreeConnector(c);
	}
	
	// Free DRM resources
	drmModeFreeResources(res);

	return ret;
}

int free_drm(vanilla_drm_ctx_t *ctx)
{
	// Close DRM
	drmClose(ctx->fd);
}