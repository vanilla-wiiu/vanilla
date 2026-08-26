#include "ui_sdl_drm.h"

#include <SDL2/SDL_syswm.h>
#include <drm_fourcc.h>
#include <drm_mode.h>
#include <errno.h>
#include <libavutil/hwcontext_drm.h>
#include <stdint.h>
#include <string.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "platform.h"

#ifndef DRM_CAP_ATOMIC_ASYNC_PAGE_FLIP
#define DRM_CAP_ATOMIC_ASYNC_PAGE_FLIP 0x15
#endif

#define MAX_HANDLE_CACHE 32
typedef struct {
    int fd;
    uint32_t handle;
} vanilla_drm_handle_t;

typedef struct vanilla_drm_ctx_t {
    int fd;
    uint32_t crtc;
    int crtc_index;
    uint32_t crtc_width;
    uint32_t crtc_height;
    uint32_t plane_id;
    int got_plane;
    uint32_t plane_fb_id_property;
    int atomic_async;
    uint32_t fb_id;
    int got_fb;
    uint32_t frame_width;
    uint32_t frame_height;
    uint32_t frame_format;
    int32_t dst_x;
    int32_t dst_y;
    uint32_t dst_width;
    uint32_t dst_height;
    vanilla_drm_handle_t handle_cache[MAX_HANDLE_CACHE];
    size_t handle_cache_count;
} vanilla_drm_ctx_t;

static uint64_t get_object_property(const int drmfd, const uint32_t object_id, const uint32_t object_type,
                                    const char *name, uint32_t *property_id)
{
    drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(drmfd, object_id, object_type);
    if (!props) {
        return 0;
    }

    uint64_t value = 0;
    for (uint32_t i = 0; i < props->count_props; i++) {
        drmModePropertyPtr prop = drmModeGetProperty(drmfd, props->props[i]);
        if (!prop) {
            continue;
        }
        if (strcmp(prop->name, name) == 0) {
            if (property_id) {
                *property_id = prop->prop_id;
            }
            value = props->prop_values[i];
            drmModeFreeProperty(prop);
            break;
        }
        drmModeFreeProperty(prop);
    }

    drmModeFreeObjectProperties(props);
    return value;
}

static int find_plane(const int drmfd, const int crtcidx, const uint32_t format, const int require_atomic,
                      uint32_t *const pplane_id, uint32_t *const pfb_id_property)
{
    drmModePlaneResPtr planes;
    drmModePlanePtr plane;
    unsigned int i;
    unsigned int j;
    int ret = 0;

    planes = drmModeGetPlaneResources(drmfd);
    if (!planes) {
        vpilog("drmModeGetPlaneResources failed: %s\n", strerror(errno));
        return -1;
    }

    for (i = 0; i < planes->count_planes; ++i) {
        plane = drmModeGetPlane(drmfd, planes->planes[i]);
        if (!plane) {
            vpilog("drmModeGetPlane failed: %s\n", strerror(errno));
            continue;
        }

        if (!(plane->possible_crtcs & (1 << crtcidx))) {
            drmModeFreePlane(plane);
            continue;
        }

        for (j = 0; j < plane->count_formats; ++j) {
            if (plane->formats[j] == format)
                break;
        }

        if (j == plane->count_formats) {
            drmModeFreePlane(plane);
            continue;
        }

        uint32_t fb_id_property = 0;
        uint32_t type_property = 0;
        uint64_t plane_type =
            get_object_property(drmfd, plane->plane_id, DRM_MODE_OBJECT_PLANE, "type", &type_property);
        if (type_property && plane_type != DRM_PLANE_TYPE_OVERLAY) {
            drmModeFreePlane(plane);
            continue;
        }
        if (require_atomic) {
            get_object_property(drmfd, plane->plane_id, DRM_MODE_OBJECT_PLANE, "FB_ID", &fb_id_property);
            if (!fb_id_property) {
                drmModeFreePlane(plane);
                continue;
            }
        }

        *pplane_id = plane->plane_id;
        *pfb_id_property = fb_id_property;
        drmModeFreePlane(plane);
        break;
    }

    if (i == planes->count_planes) {
        ret = -1;
    }

    drmModeFreePlaneResources(planes);
    return ret;
}

static int set_plane_async(vanilla_drm_ctx_t *ctx, uint32_t fb_id)
{
    drmModeAtomicReqPtr req = drmModeAtomicAlloc();
    if (!req) {
        return -1;
    }

    int ret = drmModeAtomicAddProperty(req, ctx->plane_id, ctx->plane_fb_id_property, fb_id);
    if (ret >= 0) {
        ret = drmModeAtomicCommit(ctx->fd, req, DRM_MODE_PAGE_FLIP_ASYNC, NULL);
    }

    drmModeAtomicFree(req);
    return ret;
}

static void fit_frame_to_crtc(const vanilla_drm_ctx_t *ctx, uint32_t frame_width, uint32_t frame_height, int32_t *dst_x,
                              int32_t *dst_y, uint32_t *dst_width, uint32_t *dst_height)
{
    if (!ctx->crtc_width || !ctx->crtc_height || !frame_width || !frame_height) {
        *dst_x = 0;
        *dst_y = 0;
        *dst_width = frame_width;
        *dst_height = frame_height;
        return;
    }

    if ((uint64_t) ctx->crtc_width * frame_height > (uint64_t) ctx->crtc_height * frame_width) {
        *dst_height = ctx->crtc_height;
        *dst_width = (uint64_t) ctx->crtc_height * frame_width / frame_height;
        *dst_x = (ctx->crtc_width - *dst_width) / 2;
        *dst_y = 0;
    } else {
        *dst_width = ctx->crtc_width;
        *dst_height = (uint64_t) ctx->crtc_width * frame_height / frame_width;
        *dst_x = 0;
        *dst_y = (ctx->crtc_height - *dst_height) / 2;
    }
}

int vui_sdl_drm_present(vanilla_drm_ctx_t *ctx, AVFrame *frame)
{
    const AVDRMFrameDescriptor *desc = (AVDRMFrameDescriptor *) frame->data[0];
    const uint32_t format = desc->layers[0].format;

    if (!ctx->got_plane) {
        int found =
            find_plane(ctx->fd, ctx->crtc_index, format, ctx->atomic_async, &ctx->plane_id, &ctx->plane_fb_id_property);
        if (found < 0 && ctx->atomic_async) {
            ctx->atomic_async = 0;
            found = find_plane(ctx->fd, ctx->crtc_index, format, 0, &ctx->plane_id, &ctx->plane_fb_id_property);
        }
        if (found < 0) {
            vpilog("Failed to find plane for format: %x\n", format);
            return 0;
        } else {
            ctx->got_plane = 1;
        }
    }

    uint32_t pitches[AV_DRM_MAX_PLANES] = {0};
    uint32_t offsets[AV_DRM_MAX_PLANES] = {0};
    uint32_t bo_handles[AV_DRM_MAX_PLANES] = {0};
    uint64_t modifiers[AV_DRM_MAX_PLANES] = {0};
    uint32_t handles[AV_DRM_MAX_PLANES];

    for (int i = 0; i < desc->nb_objects; i++) {
        int fd = desc->objects[i].fd;

        uint32_t handle = 0;
        for (size_t i = 0; i < ctx->handle_cache_count; i++) {
            vanilla_drm_handle_t *h = &ctx->handle_cache[i];
            if (h->fd == fd) {
                handle = h->handle;
                break;
            }
        }

        if (!handle) {
            vpilog("(re)opening DRM handle because fd 0x%x was not found\n", fd);

            vanilla_drm_handle_t *h = &ctx->handle_cache[ctx->handle_cache_count];
            if (drmPrimeFDToHandle(ctx->fd, fd, &handle) != 0) {
                vpilog("Failed to get handle from file descriptor: %s\n", strerror(errno));
                return 0;
            }
            h->handle = handle;
            h->fd = fd;
            ctx->handle_cache_count++;
        }

        handles[i] = handle;
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
            bo_handles[n] = handles[plane->object_index];

            n++;
        }
    }

    uint32_t new_fb;
    if (drmModeAddFB2WithModifiers(ctx->fd, frame->width, frame->height, desc->layers[0].format, bo_handles, pitches,
                                   offsets, modifiers, &new_fb, DRM_MODE_FB_MODIFIERS) != 0) {
        vpilog("Failed to create framebuffer: %s\n", strerror(errno));
        return 0;
    }

    int32_t dst_x;
    int32_t dst_y;
    uint32_t dst_width;
    uint32_t dst_height;
    fit_frame_to_crtc(ctx, frame->width, frame->height, &dst_x, &dst_y, &dst_width, &dst_height);

    int geometry_changed = !ctx->got_fb || ctx->frame_width != (uint32_t) frame->width ||
                           ctx->frame_height != (uint32_t) frame->height || ctx->frame_format != format ||
                           ctx->dst_x != dst_x || ctx->dst_y != dst_y || ctx->dst_width != dst_width ||
                           ctx->dst_height != dst_height;

    int presented = 0;
    if (!geometry_changed && ctx->atomic_async) {
        if (set_plane_async(ctx, new_fb) == 0) {
            presented = 1;
        } else {
            /*
             * Some drivers advertise atomic async flips but reject a
             * particular plane configuration. Fall back permanently rather
             * than logging and retrying the unsupported path every frame.
             */
            vpilog("Atomic async plane update failed, using vblank updates: %s\n", strerror(errno));
            ctx->atomic_async = 0;
        }
    }

    if (!presented && drmModeSetPlane(ctx->fd, ctx->plane_id, ctx->crtc, new_fb, 0, dst_x, dst_y, dst_width, dst_height,
                                      0, 0, frame->width << 16, frame->height << 16) != 0) {
        vpilog("Failed to set plane: %s\n", strerror(errno));
        drmModeRmFB(ctx->fd, new_fb);
        return 0;
    }

    // Free old framebuffer
    if (ctx->got_fb) {
        drmModeRmFB(ctx->fd, ctx->fb_id);
    }
    ctx->fb_id = new_fb;
    ctx->got_fb = 1;
    ctx->frame_width = frame->width;
    ctx->frame_height = frame->height;
    ctx->frame_format = format;
    ctx->dst_x = dst_x;
    ctx->dst_y = dst_y;
    ctx->dst_width = dst_width;
    ctx->dst_height = dst_height;

    return 1;
}

int vui_sdl_drm_initialize(vanilla_drm_ctx_t **c, SDL_Window *window)
{
    *c = NULL;
    vanilla_drm_ctx_t *ctx = (vanilla_drm_ctx_t *) malloc(sizeof(vanilla_drm_ctx_t));
    if (!ctx) {
        vpilog("Failed to allocate DRM context\n");
        return 0;
    }

    memset(ctx, 0, sizeof(vanilla_drm_ctx_t));

    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (!SDL_GetWindowWMInfo(window, &wmi) || wmi.subsystem != SDL_SYSWM_KMSDRM) {
        free(ctx);
        return 0;
    }

    ctx->fd = wmi.info.kmsdrm.drm_fd;

    int ret = 0;
    uint64_t async_cap = 0;
    if (drmGetCap(ctx->fd, DRM_CAP_ATOMIC_ASYNC_PAGE_FLIP, &async_cap) == 0 && async_cap &&
        drmSetClientCap(ctx->fd, DRM_CLIENT_CAP_ATOMIC, 1) == 0) {
        ctx->atomic_async = 1;
        vpilog("DRM atomic async plane updates enabled\n");
    } else {
        vpilog("DRM atomic async plane updates unavailable; updates will wait for vblank\n");
    }

    // Find DRM output
    drmModeResPtr res = drmModeGetResources(ctx->fd);
    if (!res) {
        vpilog("Failed to get DRM resources: %s\n", strerror(errno));
        free(ctx);
        return 0;
    }

    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnectorPtr c = drmModeGetConnector(ctx->fd, res->connectors[i]);
        if (c && c->encoder_id) {
            drmModeEncoderPtr enc = drmModeGetEncoder(ctx->fd, c->encoder_id);
            if (enc && enc->crtc_id) {
                drmModeCrtcPtr crtc = drmModeGetCrtc(ctx->fd, enc->crtc_id);

                if (crtc) {
                    // Good! We can use this connector :)
                    ctx->crtc = crtc->crtc_id;
                    ctx->crtc_width = crtc->mode_valid ? crtc->mode.hdisplay : crtc->width;
                    ctx->crtc_height = crtc->mode_valid ? crtc->mode.vdisplay : crtc->height;

                    for (int j = 0; j < res->count_crtcs; j++) {
                        if (res->crtcs[j] == crtc->crtc_id) {
                            ctx->crtc_index = j;
                            break;
                        }
                    }

                    ret = ctx->crtc_width > 0 && ctx->crtc_height > 0;

                    drmModeFreeCrtc(crtc);
                }
            }
            if (enc)
                drmModeFreeEncoder(enc);
        }
        if (c)
            drmModeFreeConnector(c);
        if (ret)
            break;
    }

    // Free DRM resources
    drmModeFreeResources(res);
    if (!ret) {
        vpilog("Failed to find an active DRM output\n");
        free(ctx);
        return 0;
    }

    ctx->got_plane = 0;
    ctx->got_fb = 0;
    for (size_t i = 0; i < MAX_HANDLE_CACHE; i++) {
        vanilla_drm_handle_t *h = &ctx->handle_cache[i];
        h->fd = -1;
        h->handle = 0;
    }

    *c = ctx;
    return ret;
}

int vui_sdl_drm_free(vanilla_drm_ctx_t **c)
{
    vanilla_drm_ctx_t *ctx = *c;
    *c = NULL;

    struct drm_gem_close gem_close = {0};
    for (size_t i = 0; i < ctx->handle_cache_count; i++) {
        vanilla_drm_handle_t *h = &ctx->handle_cache[i];
        gem_close.handle = h->handle;
        drmIoctl(ctx->fd, DRM_IOCTL_GEM_CLOSE, &gem_close);
        h->handle = 0;
        h->fd = -1;
    }

    if (ctx->got_fb) {
        drmModeRmFB(ctx->fd, ctx->fb_id);
        ctx->got_fb = 0;
    }

    ctx->got_plane = 0;

    // Close DRM (now owned by SDL so don't do this)
    // drmClose(ctx->fd);

    free(ctx);

    return 0;
}
