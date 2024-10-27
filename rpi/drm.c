#include "drm.h"

#include <drm_fourcc.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#define ALIGN(x, a)		((x) + (a - 1)) & (~(a - 1))

int set_tty(int mode)
{
    /*int tty_fd = open("/dev/tty0", O_RDWR);
    if (tty_fd == -1) {
        fprintf(stderr, "Failed to open /dev/tty\n");
        return 0;
    }
    if (ioctl(tty_fd, KDSETMODE, mode) < 0) {
        fprintf(stderr, "Failed to set KDSETMODE: %s (%i)\n", strerror(errno), errno);
        return 0;
    }
    close(tty_fd);*/

    /*if (ioctl(STDIN_FILENO, KDSETMODE, mode) < 0) {
        fprintf(stderr, "Failed to set KDSETMODE: %s (%i)\n", strerror(errno), errno);
        return 0;
    }*/

    return 1;
}

int initialize_drm(vanilla_drm_ctx_t *ctx)
{
	// Open DRM
	ctx->fd = drmOpen("vc4", 0);
	if (ctx->fd == -1) {
		return 0;
	}

    if (!set_tty(KD_GRAPHICS)) {
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
				ctx->crtc = crtc->crtc_id;

                for (int j = 0; j < res->count_crtcs; j++) {
                    if (res->crtcs[j] == crtc->crtc_id) {
                        ctx->crtc_index = j;
                        break;
                    }
                }

                ret = 1;

				drmModeFreeCrtc(crtc);
			}
			drmModeFreeEncoder(enc);
		}
		drmModeFreeConnector(c);
	}
	
	// Free DRM resources
	drmModeFreeResources(res);

    ctx->got_plane = 0;
    ctx->got_fb = 0;
    ctx->got_handles = 0;
    memset(ctx->handles, 0, sizeof(ctx->handles));

	return ret;
}

int free_drm(vanilla_drm_ctx_t *ctx)
{
    if (ctx->got_fb) {
        drmModeRmFB(ctx->fd, ctx->fb_id);
    }

    set_tty(KD_TEXT);

	// Close DRM
	drmClose(ctx->fd);
}

static int find_plane(const int drmfd, const int crtcidx, const uint32_t format,
                      uint32_t *const pplane_id)
{
    drmModePlaneResPtr planes;
    drmModePlanePtr plane;
    unsigned int i;
    unsigned int j;
    int ret = 0;

    planes = drmModeGetPlaneResources(drmfd);
    if (!planes) {
        fprintf(stderr, "drmModeGetPlaneResources failed: %s\n", strerror(errno));
        return -1;
    }

    for (i = 0; i < planes->count_planes; ++i) {
        plane = drmModeGetPlane(drmfd, planes->planes[i]);
        if (!plane) {
            fprintf(stderr, "drmModeGetPlane failed: %s\n", strerror(errno));
            break;
        }

        if (!(plane->possible_crtcs & (1 << crtcidx))) {
            drmModeFreePlane(plane);
            continue;
        }

        for (j = 0; j < plane->count_formats; ++j) {
            if (plane->formats[j] == format) break;
        }

        if (j == plane->count_formats) {
            drmModeFreePlane(plane);
            continue;
        }

        *pplane_id = plane->plane_id;
        drmModeFreePlane(plane);
        break;
    }

    if (i == planes->count_planes) ret = -1;

    drmModeFreePlaneResources(planes);
    return ret;
}

extern int running;
int display_drm(vanilla_drm_ctx_t *ctx, AVFrame *frame)
{
    const AVDRMFrameDescriptor *desc = (AVDRMFrameDescriptor *) frame->data[0];
    const uint32_t format = desc->layers[0].format;

    if (!ctx->got_plane) {
        if (find_plane(ctx->fd, ctx->crtc_index, format, &ctx->plane_id) < 0) {
            fprintf(stderr, "Failed to find plane for format: %x\n", format, DRM_FORMAT_YUV420, frame->format);
            return 0;
        } else {
            ctx->got_plane = 1;
        }
    }

    /*{
        drmVBlank vbl;
        vbl.request.type = DRM_VBLANK_RELATIVE;
        vbl.request.sequence = 0;
        while (running && drmWaitVBlank(ctx->fd, &vbl)) {
            // Not sure what this does, stole it from hello_drmprime
            if (errno != EINTR) {
                break;
            }
        }
    }

    if (!running) {
        return 1;
    }*/

    uint32_t pitches[AV_DRM_MAX_PLANES] = {0};
    uint32_t offsets[AV_DRM_MAX_PLANES] = {0};
    uint32_t bo_handles[AV_DRM_MAX_PLANES] = {0};
    uint64_t modifiers[AV_DRM_MAX_PLANES] = {0};

    uint32_t new_handles[AV_DRM_MAX_PLANES] = {0};

    for (int i = 0; i < desc->nb_objects; i++) {
        if (drmPrimeFDToHandle(ctx->fd, desc->objects[i].fd, &new_handles[i]) != 0) {
            fprintf(stderr, "Failed to get handle from file descriptor: %s\n", strerror(errno));
            return 0;
        }
    }

    int n = 0;
    for (int i = 0; i < desc->nb_layers; i++) {
        const AVDRMLayerDescriptor *layer = &desc->layers[i];
        for (int j = 0; j < layer->nb_planes; j++) {
            const AVDRMPlaneDescriptor *plane = &layer->planes[j];
            const AVDRMObjectDescriptor *obj = &desc->objects[plane->object_index];

            pitches[n] = plane->pitch;
            offsets[n] = plane->offset;
            modifiers[n] = obj->format_modifier;
            bo_handles[n] = new_handles[plane->object_index];

            n++;
        }
    }

    /*fprintf(stderr, "%dx%d, fmt: %x, boh=%d,%d,%d,%d, pitch=%d,%d,%d,%d,"
               " offset=%d,%d,%d,%d, mod=%llx,%llx,%llx,%llx\n",
               frame->width,
               frame->height,
               desc->layers[0].format,
               bo_handles[0],
               bo_handles[1],
               bo_handles[2],
               bo_handles[3],
               pitches[0],
               pitches[1],
               pitches[2],
               pitches[3],
               offsets[0],
               offsets[1],
               offsets[2],
               offsets[3],
               (long long)modifiers[0],
               (long long)modifiers[1],
               (long long)modifiers[2],
               (long long)modifiers[3]
              );*/

    uint32_t new_fb;
    if (drmModeAddFB2WithModifiers(ctx->fd,
                                   frame->width, frame->height, desc->layers[0].format,
                                   bo_handles, pitches, offsets, modifiers,
                                   &new_fb, DRM_MODE_FB_MODIFIERS) != 0) {
        fprintf(stderr, "Failed to create framebuffer: %s\n", strerror(errno));
        return 0;
    }

    if (drmModeSetPlane(ctx->fd, ctx->plane_id, ctx->crtc, new_fb, 0,
                    0, 0, frame->width, frame->height,
                    0, 0, frame->width << 16, frame->height << 16) != 0) {
        fprintf(stderr, "Failed to set plane: %s\n", strerror(errno));
        return 0;
    }

    // Free old framebuffer
    if (ctx->got_fb) {
        drmModeRmFB(ctx->fd, ctx->fb_id);
    }
    ctx->fb_id = new_fb;
    ctx->got_fb = 1;

    if (ctx->got_handles) {
        struct drm_gem_close gem_close = {0};
        for (int i = 0; i < desc->nb_objects; i++) {
            if (ctx->handles[i]) {
                gem_close.handle = ctx->handles[i];
                drmIoctl(ctx->fd, DRM_IOCTL_GEM_CLOSE, &gem_close);
                ctx->handles[i] = 0;
            }
        }
    }
    memcpy(ctx->handles, new_handles, sizeof(ctx->handles));
    ctx->got_handles = 1;

    return 1;
}