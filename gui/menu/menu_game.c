#include "menu_game.h"

#include <assert.h>
#include <limits.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
#include <stdatomic.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <vanilla.h>

#include "config.h"
#include "lang.h"
#include "menu_common.h"
#include "menu_main.h"
#include "ui/ui_anim.h"
#include "ui/ui_util.h"

#ifdef VANILLA_V4L2REQUEST_AVAILABLE
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/videodev2.h>
#endif

static char vpi_toast_string[VPI_TOAST_MAX_LEN];
static struct timeval vpi_toast_expiry;
static int vpi_toast_number = 0;

// Arbitrary limits so loops don't go on forever
static const long VPI_DECODE_POLL_US = 500;
static const long VPI_DECODE_MAX_POLLS = 40;

AVFrame *vpi_present_frame = 0;
pthread_mutex_t vpi_present_frame_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t vpi_present_frame_cond = PTHREAD_COND_INITIALIZER;
uint64_t vpi_present_frame_sequence = 0;

static AVFormatContext *recording_fmt_ctx = 0;
static AVStream *recording_vstr;
static AVStream *recording_astr;
static struct timeval recording_start;
static char screenshot_buf[4096] = {0};

static const int VIDEO_STREAM_INDEX = 0;
static const int AUDIO_STREAM_INDEX = 1;

static pthread_t vpi_event_thread;

int vpi_egl_available = 0;

static int status_lbl;
static struct {
    int64_t last_power_time;
} menu_game_ctx;

static _Atomic int vpi_game_queued_error = VANILLA_SUCCESS;

void vpi_menu_game_exit(vui_context_t *vui, void *v)
{
    if (vpi_config.autoconnect == -1) {
        vpi_menu_main(vui, v);
    } else {
        vpi_menu_do_quit(vui, v);
    }
}

void back_to_main_menu(vui_context_t *vui, int button, void *v)
{
    int layer = (intptr_t) v;
    vui_transition_fade_layer_out(vui, layer, vpi_menu_game_exit, (void *) (intptr_t) 1);
}

void show_error(vui_context_t *vui, void *v)
{
    vui_reset(vui);

    vui_enable_background(vui, 1);

    int scrw, scrh;
    vui_get_screen_size(vui, &scrw, &scrh);

    int layer = vui_layer_create(vui);

    vui_label_create(vui, 0, scrh * 1 / 6, scrw, scrh, lang(VPI_LANG_ERROR), vui_color_create(0.25f,0.25f,0.25f,1), VUI_FONT_SIZE_NORMAL, layer);

    char buf[10];
    snprintf(buf, sizeof(buf), "%i", (int) (intptr_t) v);
    vui_label_create(vui, 0, scrh * 3 / 6, scrw, scrh, buf, vui_color_create(1,0,0,1), VUI_FONT_SIZE_NORMAL, layer);

    const int ok_btn_w = BTN_SZ*2;
    vui_button_create(vui, scrw/2 - ok_btn_w/2, scrh * 3 / 4, ok_btn_w, BTN_SZ, lang(VPI_LANG_OK_BTN), 0, VUI_BUTTON_STYLE_BUTTON, layer, back_to_main_menu, (void *) (intptr_t) layer);

    vui_transition_fade_black_out(vui, 0, 0);
}

void cancel_connect(vui_context_t *vui, int button, void *v)
{
    int layer = (intptr_t) v;
    vanilla_stop();
    vui_transition_fade_layer_out(vui, layer, vpi_menu_game_exit, 0);
}

static void update_battery_information(vui_context_t *vui, int64_t time)
{
    // Get power information
    int64_t current_minute = time / (2 * 1000000); // Update every 2 seconds
    if (menu_game_ctx.last_power_time != current_minute) {
        int percent;
        vui_power_state_t power_state = vui_power_state_get(vui, &percent);

        enum VanillaBatteryStatus status = VANILLA_BATTERY_STATUS_UNKNOWN;

        switch (power_state) {
        case VUI_POWERSTATE_ERROR:
        case VUI_POWERSTATE_UNKNOWN:
            // Sending an unknown/error state to the Wii U can cause undesirable
            // side effects (e.g. it will be unable to "update" the gamepad and
            // will nag the user every boot about it), so instead we just send
            // it an innocuous "charging" state.
        case VUI_POWERSTATE_NO_BATTERY:
        case VUI_POWERSTATE_CHARGING:
        case VUI_POWERSTATE_CHARGED:
            // Plugged in
            status = VANILLA_BATTERY_STATUS_CHARGING;
            break;
        case VUI_POWERSTATE_ON_BATTERY:
            // On battery
            status = (percent < 10) ? VANILLA_BATTERY_STATUS_VERY_LOW :
                        (percent < 25) ? VANILLA_BATTERY_STATUS_LOW :
                        (percent < 75) ? VANILLA_BATTERY_STATUS_MEDIUM :
                        (percent < 95) ? VANILLA_BATTERY_STATUS_HIGH :
                        VANILLA_BATTERY_STATUS_FULL;
            break;
        }

        vanilla_set_battery_status(status);

        menu_game_ctx.last_power_time = current_minute;
    }
}

static inline void be16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }

size_t build_avcc(uint8_t *dst, size_t cap,
                  const uint8_t * const *sps_list, const uint16_t *sps_len, int sps_count,
                  const uint8_t * const *pps_list, const uint16_t *pps_len, int pps_count,
                  int nal_length_size /* 1,2,4 */)
{
    assert(nal_length_size==1 || nal_length_size==2 || nal_length_size==4);

    // Derive profile/compat/level from the first SPS (bytes after NAL header)
    // SPS layout starts with: [nal_header=0x67][profile_idc][profile_compat][level_idc]...
    if (sps_count < 1 || sps_len[0] < 4) return 0;
    const uint8_t *sps0 = sps_list[0];
    uint8_t AVCProfileIndication  = sps0[1];
    uint8_t profile_compatibility = sps0[2];
    uint8_t AVCLevelIndication    = sps0[3];

    uint8_t *p = dst;
    uint8_t *e = dst + cap;

    // configurationVersion
    if (p+1 > e) return 0; *p++ = 1;
    // profile/compat/level
    if (p+3 > e) return 0;
    *p++ = AVCProfileIndication;
    *p++ = profile_compatibility;
    *p++ = AVCLevelIndication;

    // lengthSizeMinusOne (low 2 bits), upper 6 bits all 1s
    if (p+1 > e) return 0;
    uint8_t lengthSizeMinusOne = (uint8_t)(nal_length_size - 1); // 0→1B,1→2B,3→4B
    *p++ = (uint8_t)(0xFC | (lengthSizeMinusOne & 0x03));

    // numOfSPS (low 5 bits), upper 3 bits all 1s
    if (sps_count > 31) return 0;
    if (p+1 > e) return 0;
    *p++ = (uint8_t)(0xE0 | (sps_count & 0x1F));

    // Write each SPS: [2B len][bytes...]
    for (int i = 0; i < sps_count; ++i) {
        if (p+2+sps_len[i] > e) return 0;
        be16(p, sps_len[i]); p += 2;
        memcpy(p, sps_list[i], sps_len[i]); p += sps_len[i];
    }

    // numOfPPS
    if (pps_count > 255) return 0;
    if (p+1 > e) return 0;
    *p++ = (uint8_t)pps_count;

    // Write each PPS: [2B len][bytes...]
    for (int i = 0; i < pps_count; ++i) {
        if (p+2+pps_len[i] > e) return 0;
        be16(p, pps_len[i]); p += 2;
        memcpy(p, pps_list[i], pps_len[i]); p += pps_len[i];
    }

    return (size_t)(p - dst);
}

static enum AVPixelFormat nvdec_get_format(struct AVCodecContext *s, const enum AVPixelFormat *fmt)
{
	while (*fmt != AV_PIX_FMT_NONE) {
        if (*fmt == AV_PIX_FMT_CUDA) {
            return *fmt;
		}
        fmt++;
    }

    return AV_PIX_FMT_NONE;
}

static enum AVPixelFormat vaapi_get_format(struct AVCodecContext *s, const enum AVPixelFormat *fmt)
{
	while (*fmt != AV_PIX_FMT_NONE) {
        if (*fmt == AV_PIX_FMT_VAAPI) {
            return *fmt;
		}
        fmt++;
    }

    return AV_PIX_FMT_NONE;
}

static enum AVPixelFormat drm_get_format(struct AVCodecContext *s, const enum AVPixelFormat *fmt)
{
	while (*fmt != AV_PIX_FMT_NONE) {
        if (*fmt == AV_PIX_FMT_DRM_PRIME) {
            return *fmt;
		}
        fmt++;
    }

    return AV_PIX_FMT_NONE;
}

typedef struct {
    const char *name;
    const AVCodec *codec;
    enum AVPixelFormat (*get_format)(struct AVCodecContext *s, const enum AVPixelFormat * fmt);
} hwdec_t;

enum HwDecoderType {
    HWDEC_TYPE_NVDEC,
    HWDEC_TYPE_VAAPI,
    HWDEC_TYPE_V4L2REQUEST,
    HWDEC_TYPE_DRM,
    HWDEC_TYPE_SOFTWARE,
    HWDEC_TYPE_COUNT
};

#ifdef VANILLA_V4L2REQUEST_AVAILABLE
static int is_v4l2request_available(void)
{
    char path[32];
    for (int i = 0; i < 64; i++) {
        snprintf(path, sizeof(path), "/dev/video%d", i);
        int fd = open(path, O_RDWR | O_NONBLOCK);
        if (fd < 0) {
            continue;
        }

        int found = 0;
        for (int t = 0; t < 2; t++) {
            struct v4l2_fmtdesc desc;
            memset(&desc, 0, sizeof(desc));
            desc.type = (t == 0) ? V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE
                                 : V4L2_BUF_TYPE_VIDEO_OUTPUT;
            for (desc.index = 0; ioctl(fd, VIDIOC_ENUM_FMT, &desc) == 0; desc.index++) {
                if (desc.pixelformat == V4L2_PIX_FMT_H264_SLICE) {
                    found = 1;
                    break;
                }
            }

            if (found) {
                break;
            }
        }

        close(fd);
        if (found) {
            return 1;
        }
    }
    return 0;
}
#endif // VANILLA_V4L2REQUEST_AVAILABLE

void vpi_decode_exit(vpi_decode_state_t *s)
{
    for (size_t i = 0; i < VPI_DECODE_QUEUE_CAPACITY; i++) {
        av_packet_free(&s->packet_queue[i]);
    }
    s->packet_queue_read = 0;
    s->packet_queue_write = 0;
    s->packet_queue_count = 0;

    if (s->cond_init) {
        pthread_cond_destroy(&s->cond);
        s->cond_init = 0;
    }

    if (s->mutex_init) {
        pthread_mutex_destroy(&s->mutex);
        s->mutex_init = 0;
    }

    if (s->frame)
        av_frame_free(&s->frame);

    pthread_mutex_lock(&vpi_present_frame_mutex);
    if (vpi_present_frame) {
	    av_frame_free(&vpi_present_frame);
        vpi_present_frame_sequence++;
        pthread_cond_broadcast(&vpi_present_frame_cond);
    }
    pthread_mutex_unlock(&vpi_present_frame_mutex);

    if (s->codec_ctx)
        avcodec_free_context(&s->codec_ctx);

	if (s->hw_device_ctx)
		av_buffer_unref(&s->hw_device_ctx);
}

int open_decoder(vpi_decode_state_t *s, hwdec_t *dec)
{
    int ffmpeg_err;

	// If we couldn't find a hardware or software decoder, fail
	if (!dec->codec) {
		vpilog("No decoder was available\n");
        return VANILLA_ERR_GENERIC;
	}

    s->codec_ctx = avcodec_alloc_context3(dec->codec);
	if (!s->codec_ctx) {
		vpilog("Failed to allocate codec context\n");
        return VANILLA_ERR_GENERIC;
	}

    if (s->hw_device_ctx) {
        s->codec_ctx->hw_device_ctx = av_buffer_ref(s->hw_device_ctx);
	}

	if (dec->get_format) {
		s->codec_ctx->get_format = dec->get_format;
	}

    const int BUILD_AVCC = 0;
    if (BUILD_AVCC) {
        uint8_t sps[100], pps[100];
        uint16_t sps_size = vanilla_generate_sps_params(sps, sizeof(sps));
        uint16_t pps_size = vanilla_generate_pps_params(pps, sizeof(pps));

        const uint8_t *p_sps = sps;
        const uint8_t *p_pps = pps;

        s->codec_ctx->extradata = av_malloc(1024);
        s->codec_ctx->extradata_size = build_avcc(
            s->codec_ctx->extradata, 1024,
            &p_sps, &sps_size, 1,
            &p_pps, &pps_size, 1,
            4
        );
    }

	ffmpeg_err = avcodec_open2(s->codec_ctx, dec->codec, NULL);
    if (ffmpeg_err < 0) {
		vpilog("Failed to open decoder: %i\n", ffmpeg_err);
        return VANILLA_ERR_GENERIC;
	}

    vpilog("Decoding: %s\n", dec->name);

    return VANILLA_SUCCESS;
}

int vpi_decode_init(vpi_decode_state_t *s)
{
    int ffmpeg_err;

    // av_log_set_level(AV_LOG_VERBOSE);

    // Initialize decoding context, preferring hardware decoding when available
    hwdec_t decoders[HWDEC_TYPE_COUNT];

    decoders[HWDEC_TYPE_NVDEC].name = "NVDEC";
    decoders[HWDEC_TYPE_NVDEC].codec = avcodec_find_decoder_by_name("h264_cuvid");
    decoders[HWDEC_TYPE_NVDEC].get_format = nvdec_get_format;

    decoders[HWDEC_TYPE_VAAPI].name = "VAAPI";
    decoders[HWDEC_TYPE_VAAPI].codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    decoders[HWDEC_TYPE_VAAPI].get_format = vaapi_get_format;

    decoders[HWDEC_TYPE_V4L2REQUEST].name = "V4L2 Request";
    decoders[HWDEC_TYPE_V4L2REQUEST].codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    decoders[HWDEC_TYPE_V4L2REQUEST].get_format = drm_get_format;

    decoders[HWDEC_TYPE_DRM].name = "DRM";
    decoders[HWDEC_TYPE_DRM].codec = avcodec_find_decoder_by_name("h264_v4l2m2m");
    decoders[HWDEC_TYPE_DRM].get_format = drm_get_format;

    decoders[HWDEC_TYPE_SOFTWARE].name = "Software";
    decoders[HWDEC_TYPE_SOFTWARE].codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    decoders[HWDEC_TYPE_SOFTWARE].get_format = 0;

    // Discover the most ideal hardware decoder
    int r = VANILLA_ERR_GENERIC;

    if (!vpi_config.force_software_decode) {
#ifdef VANILLA_CUDA_AVAILABLE
        // See if we can create an NVDEC context (most NVIDIA GPUs)
        if (r != VANILLA_SUCCESS) {
            vpi_decode_exit(s);
            if (((ffmpeg_err = av_hwdevice_ctx_create(&s->hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, 0, 0, 0)) >= 0)) {
                r = open_decoder(s, &decoders[HWDEC_TYPE_NVDEC]);
            }
        }
#endif // VANILLA_CUDA_AVAILABLE

        // See if we can create a VAAPI context (most Linux systems)
        if (r != VANILLA_SUCCESS) {
            vpi_decode_exit(s);
            if (vpi_egl_available
                && ((ffmpeg_err = av_hwdevice_ctx_create(&s->hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI, 0, 0, 0)) >= 0)) {
                r = open_decoder(s, &decoders[HWDEC_TYPE_VAAPI]);
            }
        }

#ifdef VANILLA_V4L2REQUEST_AVAILABLE
        if (r != VANILLA_SUCCESS && is_v4l2request_available()) {
            vpi_decode_exit(s);
            if ((ffmpeg_err = av_hwdevice_ctx_create(&s->hw_device_ctx, AV_HWDEVICE_TYPE_V4L2REQUEST, NULL, NULL, 0)) >= 0) {
                r = open_decoder(s, &decoders[HWDEC_TYPE_V4L2REQUEST]);
            }
        }
#endif // VANILLA_V4L2REQUEST_AVAILABLE

        // See if we can create a DRM context (Raspberry Pi, et al.)
        if (r != VANILLA_SUCCESS) {
            vpi_decode_exit(s);
            if ((ffmpeg_err = av_hwdevice_ctx_create(&s->hw_device_ctx, AV_HWDEVICE_TYPE_DRM, "/dev/dri/card0", 0, 0)) >= 0) {
                r = open_decoder(s, &decoders[HWDEC_TYPE_DRM]);
            }
        }
    }

    // Finally, fallback to a software decoder.
    if (r != VANILLA_SUCCESS) {
        vpi_decode_exit(s);
        r = open_decoder(s, &decoders[HWDEC_TYPE_SOFTWARE]);
    }

    // We don't expect the software decoder to fail, but just in case.
    if (r != VANILLA_SUCCESS) {
        vpi_decode_exit(s);
        return r;
    }

    pthread_mutex_lock(&vpi_present_frame_mutex);
	vpi_present_frame = av_frame_alloc();
    pthread_mutex_unlock(&vpi_present_frame_mutex);
	if (!vpi_present_frame) {
		vpilog("Failed to allocate AVFrame\n");
        goto fail;
	}

	s->frame = av_frame_alloc();
    if (!s->frame) {
		vpilog("Failed to allocate AVFrame\n");
        goto fail;
    }

    if (pthread_mutex_init(&s->mutex, 0) != 0) {
		vpilog("Failed to init decoder mutex\n");
        goto fail;
    }

    s->mutex_init = 1;

    if (pthread_cond_init(&s->cond, 0) != 0) {
		vpilog("Failed to init decoder condition variable\n");
        goto fail;
    }

    s->cond_init = 1;

    return VANILLA_SUCCESS;

fail:
    vpi_decode_exit(s);
    return VANILLA_ERR_GENERIC;
}

int64_t get_recording_timestamp(AVRational timebase)
{
	struct timeval tv;
	gettimeofday(&tv, 0);

	int64_t us = (tv.tv_sec - recording_start.tv_sec) * 1000000 + (tv.tv_usec - recording_start.tv_usec);
    return av_rescale_q(us, (AVRational) {1, 1000000}, timebase);
}

int dump_frame_to_file(const AVFrame *frame, const char *filename)
{
	AVFormatContext *ofmt;
	int ret = avformat_alloc_output_context2(&ofmt, 0, 0, filename);
	if (ret < 0) {
		vpilog("Failed to allocate new AVFormatContext for screenshot\n");
		goto exit;
	}

	enum AVCodecID codec_id = AV_CODEC_ID_PNG;
	const AVCodec *codec = avcodec_find_encoder(codec_id);
	if (!codec) {
		vpilog("Failed to find encoder for screenshot\n");
        ret = AVERROR_ENCODER_NOT_FOUND;
		goto exit_and_free_avformatcontext;
	}

	AVStream *s = avformat_new_stream(ofmt, 0);
	if (!s) {
		vpilog("Failed to create new AVStream for screenshot\n");
        ret = AVERROR(ENOMEM);
		goto exit_and_free_avformatcontext;
	}

	s->id = 0;

	AVCodecContext *ocodec_ctx = avcodec_alloc_context3(codec);
	if (!ocodec_ctx) {
		vpilog("Failed to create new AVCodecContext for screenshot\n");
        ret = AVERROR(ENOMEM);
		goto exit_and_free_avformatcontext;
	}

	enum AVPixelFormat opix_fmt = AV_PIX_FMT_RGB24;

	ocodec_ctx->width = frame->width;
	ocodec_ctx->height = frame->height;
	ocodec_ctx->pix_fmt = opix_fmt;
	ocodec_ctx->codec_id = codec_id;
	ocodec_ctx->time_base = (AVRational){1, 1};

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(60, 2, 100)
	ocodec_ctx->frame_num = 1;
#else
	ocodec_ctx->frame_number = 1;
#endif

	if (ofmt->flags & AVFMT_GLOBALHEADER) {
		ocodec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	AVPacket *opkt = av_packet_alloc();
	if (!opkt) {
		vpilog("Failed to create new AVPacket for screenshot\n");
        ret = AVERROR(ENOMEM);
		goto exit_and_free_avcodeccontext;
	}

	ret = avcodec_open2(ocodec_ctx, codec, 0);
	if (ret < 0) {
		vpilog("Failed to open encoder for screenshot\n");
		goto exit_and_free_avpacket;
	}

	ret = avcodec_parameters_from_context(s->codecpar, ocodec_ctx);
	if (ret < 0) {
		vpilog("Failed to copy params from context for screenshot\n");
		goto exit_and_free_avpacket;
	}

	ret = avformat_write_header(ofmt, 0);
	if (ret < 0) {
		vpilog("Failed to write header for screenshot\n");
		goto exit_and_free_avpacket;
	}

	AVFrame *swframe = 0;
	if (frame->format != AV_PIX_FMT_YUV420P) {
		// Assume this frame is hardware accelerated and we need to download it
		// to CPU memory
		swframe = av_frame_alloc();
		if (!swframe) {
			vpilog("Failed to allocate CPU frame for screenshot\n");
			ret = AVERROR(EINVAL);
			goto exit_and_free_avpacket;
		}

		if (av_hwframe_transfer_data(swframe, frame, 0) < 0) {
			vpilog("Failed to transfer data from hardware for screenshot\n");
			ret = AVERROR(EINVAL);
			goto exit_and_free_swframe;
		}

		// Use this CPU frame instead from now on
		frame = swframe;
	}

	struct SwsContext *osws_ctx = sws_getContext(
		frame->width, frame->height, frame->format,
		frame->width, frame->height, opix_fmt,
		0, 0, 0, 0);
	if (!osws_ctx) {
		vpilog("Failed to get SwsContext for screenshot\n");
        ret = AVERROR(EINVAL);
		goto exit_and_free_avpacket;
	}

	AVFrame *oframe = av_frame_alloc();
	if (!oframe) {
		vpilog("Failed to allocate AVFrame for screenshot\n");
        ret = AVERROR(ENOMEM);
		goto exit_and_free_swscontext;
	}

	oframe->width = frame->width;
	oframe->height = frame->height;
	oframe->format = opix_fmt;

	ret = av_frame_get_buffer(oframe, 0);
	if (ret < 0) {
		vpilog("Failed to get AVFrame buffer for screenshot\n");
		goto exit_and_free_frame;
	}

	ret = sws_scale(osws_ctx, (const uint8_t **) frame->data, (const int *) frame->linesize, 0, frame->height, oframe->data, oframe->linesize);
	if (ret < 0) {
		vpilog("Failed to do swscale for screenshot\n");
		goto exit_and_free_frame;
	}

	ret = avcodec_send_frame(ocodec_ctx, oframe);
	if (ret < 0) {
		vpilog("Failed to send frame to encoder for screenshot\n");
		goto exit_and_free_frame;
	}

	ret = avcodec_receive_packet(ocodec_ctx, opkt);
	if (ret < 0) {
		vpilog("Failed to receive packet from encoder for screenshot\n");
		goto exit_and_free_frame;
	}

	ret = av_write_frame(ofmt, opkt);
	if (ret < 0) {
		vpilog("Failed to write frame for screenshot\n");
		goto exit_and_free_frame;
	}

	ret = av_write_trailer(ofmt);
	if (ret < 0) {
		vpilog("Failed to write trailer for screenshot\n");
		goto exit_and_free_frame;
	}

exit_and_free_frame:
	av_frame_free(&oframe);

exit_and_free_swscontext:
	sws_freeContext(osws_ctx);

exit_and_free_swframe:
	if (swframe) {
		av_frame_free(&swframe);
	}

exit_and_free_avpacket:
	av_packet_free(&opkt);

exit_and_free_avcodeccontext:
	avcodec_free_context(&ocodec_ctx);

exit_and_free_avformatcontext:
	avformat_free_context(ofmt);

exit:
	if (ret >= 0) {
		vpilog("Saved screenshot to \"%s\"\n", filename);

		char buf[VPI_TOAST_MAX_LEN];
		snprintf(buf, sizeof(buf), lang(VPI_LANG_SCREENSHOT_SUCCESS), filename);
		vpi_show_toast(buf);
	} else {
        char buf[VPI_TOAST_MAX_LEN];
		snprintf(buf, sizeof(buf), lang(VPI_LANG_SCREENSHOT_ERROR), ret);
		vpi_show_toast(buf);
    }

	return ret;
}

void vpi_game_shutdown()
{
    vpi_game_queued_error = VANILLA_ERR_SHUTDOWN;
}

static void vpi_publish_decoded_frame(vpi_decode_state_t *s)
{
    vui_context_t *vui = s->vui;

    pthread_mutex_lock(&vpi_present_frame_mutex);

    // Send frame out to display
    av_frame_unref(vpi_present_frame);
    av_frame_move_ref(vpi_present_frame, s->frame);
    vpi_present_frame_sequence++;

    // If user has requested a screenshot, dump the screenshot to a file
    if (screenshot_buf[0] != 0) {
        dump_frame_to_file(vpi_present_frame, screenshot_buf);
        screenshot_buf[0] = 0;
    }

    // Alert display that frame is ready
    pthread_cond_broadcast(&vpi_present_frame_cond);
    pthread_mutex_unlock(&vpi_present_frame_mutex);

    // Now that we have our first frame, switch UI to game mode if not already
    // FIXME: Not thread safe? Not a huge deal but might want to fix some day
    if (!vui_game_mode_get(vui)) {
        vui_game_mode_set(vui, 1);
        vui_audio_set_enabled(vui, 1);
    }
}

static int vpi_receive_decoded_frames(vpi_decode_state_t *s)
{
    int received = 0;

    while (s->thread_running) {
        // Attempt to receive frames from decoder
        int err = avcodec_receive_frame(s->codec_ctx, s->frame);
        if (err == AVERROR(EAGAIN) || err == AVERROR_EOF) {
            break;
        }
        if (err < 0) {
            // Return unexpected error to caller
            vpilog("Failed to receive frame from decoder: %s (%i)\n", av_err2str(err), err);
            return err;
        }

        // Received a frame, send it out for publishing
        vpi_publish_decoded_frame(s);
        received++;
    }

    return received;
}

static void vpi_decode_stop(vpi_decode_state_t *s)
{
    pthread_mutex_lock(&s->mutex);
    s->thread_running = 0;
    pthread_cond_broadcast(&s->cond);
    pthread_mutex_unlock(&s->mutex);
}

static AVPacket *vpi_decode_get_next_packet(vpi_decode_state_t *s, int wait)
{
    AVPacket *pkt = NULL;

    pthread_mutex_lock(&s->mutex);

    // Wait for incoming frames from console/event loop.
    while (s->thread_running && s->packet_queue_count == 0) {
        // Wait infinitely if caller has allowed it
        if (wait) {
            pthread_cond_wait(&s->cond, &s->mutex);
            continue;
        }

        // Otherwise, only wait for a short time before checking
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_nsec += VPI_DECODE_POLL_US * 1000;
        if (deadline.tv_nsec >= 1000000000) {
            deadline.tv_sec++;
            deadline.tv_nsec -= 1000000000;
        }
        pthread_cond_timedwait(&s->cond, &s->mutex, &deadline);
        break;
    }

    // If we got a new packet, pull it from the queue to return to caller
    if (s->thread_running && s->packet_queue_count > 0) {
        pkt = s->packet_queue[s->packet_queue_read];
        s->packet_queue[s->packet_queue_read] = NULL;
        s->packet_queue_read = (s->packet_queue_read + 1) % VPI_DECODE_QUEUE_CAPACITY;
        s->packet_queue_count--;

        // Alert event thread in case it was waiting for queue to free up
        pthread_cond_broadcast(&s->cond);
    }
    pthread_mutex_unlock(&s->mutex);

    return pkt;
}

static int vpi_decode_drain(vpi_decode_state_t *s,
                            size_t *outstanding_packets)
{
    int received = vpi_receive_decoded_frames(s);

    // Decoder either returned nothing or an error, pass it up to the caller
    if (received <= 0) {
        return received;
    }

    // Received frames, substract from outstanding_packets
    size_t frame_count = (size_t) received;

    if (*outstanding_packets > frame_count) {
        *outstanding_packets -= frame_count;
    } else {
        *outstanding_packets = 0;
    }

    return received;
}

static int vpi_decode_submit_packet(vpi_decode_state_t *s, AVPacket *pkt,
                                    size_t *outstanding_packets)
{
    int err = AVERROR_EXIT;

    for (int poll = 0; s->thread_running; poll++) {
        err = avcodec_send_packet(s->codec_ctx, pkt);
        if (err != AVERROR(EAGAIN) || poll >= VPI_DECODE_MAX_POLLS) {
            break;
        }

        int received = vpi_decode_drain(s, outstanding_packets);
        if (received < 0) {
            return received;
        }
        if (received == 0) {
            av_usleep(VPI_DECODE_POLL_US);
        }
    }

    if (!s->thread_running) {
        return AVERROR_EXIT;
    }
    if (err < 0) {
        vpilog("Failed to send packet to decoder: %s (%i)\n",
               av_err2str(err), err);
        vanilla_request_idr();
        return 0;
    }

    (*outstanding_packets)++;
    return 1;
}

void *vpi_decode_loop(void *arg)
{
    vpi_decode_state_t *s = (vpi_decode_state_t *) arg;
    size_t outstanding_packets = 0;
    int idle_output_polls = 0;

    while (s->thread_running) {
        // Wait for a packet from the event loop/console
        AVPacket *pkt = vpi_decode_get_next_packet(s, outstanding_packets == 0);

        // If we got a packet, submit it to the decoder
        if (pkt) {
            int submitted = vpi_decode_submit_packet(s, pkt, &outstanding_packets);
            av_packet_free(&pkt);

            // If there was an error, break
            if (submitted < 0) {
                break;
            }
            if (submitted > 0) {
                idle_output_polls = 0;
            }
        } else if (!s->thread_running) {
            break;
        }

        int received = vpi_decode_drain(s, &outstanding_packets);
        if (received < 0) {
            break;
        }
        if (received > 0) {
            idle_output_polls = 0;
        } else if (outstanding_packets > 0 && ++idle_output_polls >= VPI_DECODE_MAX_POLLS) {
            outstanding_packets = 0;
            idle_output_polls = 0;
        }
    }

    vpi_decode_stop(s);

    return NULL;
}

static int vpi_decode_enqueue(vpi_decode_state_t *s,
                              const uint8_t *data, size_t size)
{
    // Allocate new packet
    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        return AVERROR(ENOMEM);
    }

    // Allocate data buffers
    int err = av_new_packet(pkt, (int) size);
    if (err < 0) {
        av_packet_free(&pkt);
        return err;
    }

    // Copy data into packet and set timestamps (which some decoders may need)
    memcpy(pkt->data, data, size);
    pkt->pts = av_gettime_relative();
    pkt->dts = pkt->pts;

    // Acquire lock
    pthread_mutex_lock(&s->mutex);

    // If decoder thread is at max capacity, wait until it's processed a few more
    while (s->thread_running && s->packet_queue_count == VPI_DECODE_QUEUE_CAPACITY) {
        pthread_cond_wait(&s->cond, &s->mutex);
    }

    // Since we may have waited earlier, one last check before we queue this frame up
    if (!s->thread_running) {
        pthread_mutex_unlock(&s->mutex);
        av_packet_free(&pkt);
        return AVERROR_EXIT;
    }

    // Queue frame up for decoder and signal
    s->packet_queue[s->packet_queue_write] = pkt;
    s->packet_queue_write = (s->packet_queue_write + 1) % VPI_DECODE_QUEUE_CAPACITY;
    s->packet_queue_count++;
    pthread_cond_signal(&s->cond);
    pthread_mutex_unlock(&s->mutex);

    return 0;
}

void *vpi_event_loop(void *arg)
{
    vui_context_t *vui = (vui_context_t *) arg;
    int vpi_decode_alloc = 0;
    pthread_t decode_thread;
    vanilla_event_t event;
    vpi_decode_state_t s;

    memset(&s, 0, sizeof(s));

    while (vpi_game_queued_error != VANILLA_ERR_SHUTDOWN && vanilla_wait_event(&event)) {
        int stop = 0;

        switch (event.type) {
        case VANILLA_EVENT_VIDEO:
            // If decoder context/thread is not set up, set up now
            if (!vpi_decode_alloc) {
                int ret = vpi_decode_init(&s);
                if (ret >= 0) {
                    s.vui = vui;
                    s.thread_running = 1;

                    if (pthread_create(&decode_thread, NULL, vpi_decode_loop, &s) == 0) {
                        vpi_decode_alloc = 1;
                    } else {
                        vpilog("Failed to create decode thread\n");
                        vpi_decode_stop(&s);
                        vpi_decode_exit(&s);
                    }
                }
            }

            if (vpi_decode_alloc) {
                // If we're recording, create a packet from data and send to muxer
                if (recording_fmt_ctx) {
                    AVPacket *pkt = av_packet_alloc();

                    pkt->data = event.data;
                    pkt->size = event.size;
                    pkt->stream_index = VIDEO_STREAM_INDEX;

                    int64_t ts = get_recording_timestamp(recording_vstr->time_base);

                    pkt->dts = ts;
                    pkt->pts = ts;

                    av_interleaved_write_frame(recording_fmt_ctx, pkt);

                    av_packet_free(&pkt);
                }

                // Send data to decoder thread
                int err = vpi_decode_enqueue(&s, event.data, event.size);
                if (err < 0) {
                    vpilog("Failed to queue packet for decoder: %s (%i)\n",
                           av_err2str(err), err);
                    vanilla_request_idr();
                }
            }
            break;
        case VANILLA_EVENT_AUDIO:
            vui_audio_push(vui, event.data, event.size);

            // We send audio to vpi_decode, but not actually for decoding since
            // the audio is already uncompressed. We send it in case vpi_decode
            // is recording, so the audio can be written to the file.
            vpi_decode_send_audio(event.data, event.size);
            break;
        case VANILLA_EVENT_VIBRATE:
            vui_vibrate_set(vui, event.data[0]);
            break;
        case VANILLA_EVENT_ERROR:
        {
            int backend_err = *(int *)event.data;

            switch (backend_err) {
            case VANILLA_SUCCESS:
            case VANILLA_ERR_CONNECTED:
                vpi_game_queued_error = VANILLA_SUCCESS;
                break;
            default:
                vpi_game_queued_error = backend_err;
            };
            break;
        }
		case VANILLA_EVENT_MIC:
			vui_mic_enabled_set(vui, event.data[0]);
			break;
        }

		vanilla_free_event(&event);
    }

    if (vpi_decode_alloc) {
        vpi_decode_stop(&s);
        pthread_join(decode_thread, 0);
        vpi_decode_exit(&s);
        vpi_decode_alloc = 0;
    }

    return NULL;
}

void vpi_display_update(vui_context_t *vui, int64_t time, void *v)
{
    update_battery_information(vui, time);

    switch ((int)vpi_game_queued_error) {
    case VANILLA_SUCCESS:
        // All good, do nothing
        break;
    case VANILLA_ERR_DISCONNECTED:
        // Tell user connection has terminated and try to re-establish
        vui_label_update_text(vui, status_lbl, lang(VPI_LANG_DISCONNECTED));
        vui_audio_set_enabled(vui, 0);
        vui_game_mode_set(vui, 0);
        break;
    default:
        // Something went wrong, assume we must fail and report to user
        pthread_join(vpi_event_thread, 0);
        vanilla_stop();
        if (vpi_game_queued_error != VANILLA_ERR_SHUTDOWN) {
            show_error(vui, (void*)(intptr_t) vpi_game_queued_error);
        } else {
            vpi_menu_game_exit(vui, 0);
        }
        vpi_game_queued_error = VANILLA_SUCCESS;
    }
}

void vpi_menu_game_start(vui_context_t *vui, void *v)
{
    int console = (intptr_t) v;

    vui_reset(vui);

    vui_enable_background(vui, 0);

    menu_game_ctx.last_power_time = 0;

    // Set initial values
    vpi_console_entry_t *entry = vpi_config.connected_console_entries + console;
    vanilla_set_region(entry->region);
    
    int r = vanilla_start(vpi_config.server_address, entry->bssid, entry->psk);
    if (r != VANILLA_SUCCESS) {
        show_error(vui, (void*)(intptr_t) r);
    } else {
        char buf[100];
        snprintf(buf, sizeof(buf), lang(VPI_LANG_CONNECTING_TO), entry->name);

        int fglayer = vui_layer_create(vui);

        int scrw, scrh;
        vui_get_screen_size(vui, &scrw, &scrh);
        status_lbl = vui_label_create(vui, 0, scrh * 2 / 5, scrw, scrh, buf, vui_color_create(0.5f,0.5f,0.5f,1), VUI_FONT_SIZE_NORMAL, fglayer);

        // TODO: `state` does not get freed if
        const int cancel_btn_w = BTN_SZ*3;
        int cancel_btn = vui_button_create(vui, scrw/2 - cancel_btn_w/2, scrh * 3 / 5, cancel_btn_w, BTN_SZ, lang(VPI_LANG_CANCEL_BTN), 0, VUI_BUTTON_STYLE_BUTTON, fglayer, cancel_connect, (void *) (intptr_t) fglayer);

        vui_transition_fade_layer_in(vui, fglayer, 0, 0);

        vpi_game_queued_error = VANILLA_SUCCESS;
        pthread_create(&vpi_event_thread, 0, vpi_event_loop, vui);

        vui_start_passive_animation(vui, vpi_display_update, 0);
    }
}

void vpi_menu_game(vui_context_t *vui, void *v)
{
    vpi_menu_start_pipe(vui, 0, vpi_menu_game_start, v, 0, 0);
}

void vpi_show_toast(const char *message)
{
    vui_strncpy(vpi_toast_string, message, sizeof(vpi_toast_string));

    gettimeofday(&vpi_toast_expiry, 0);

    // Toast lasts for 2 seconds
    vpi_toast_expiry.tv_sec += 2;

    vpi_toast_number++;
}

void vpi_get_toast(int *number, char *output, size_t output_size, struct timeval *expiry_time)
{
    if (number)
        *number = vpi_toast_number;

    if (output && output_size) {
        vui_strncpy(output, vpi_toast_string, output_size);
    }

    if (expiry_time) {
        *expiry_time = vpi_toast_expiry;
    }
}

void vpi_decode_send_audio(const void *data, size_t size)
{
	if (recording_fmt_ctx) {
        int ret;

		AVPacket *pkt = av_packet_alloc();

		pkt->data = (uint8_t *) data;
		pkt->size = size;

		int64_t ts = get_recording_timestamp(recording_astr->time_base);
        pkt->stream_index = AUDIO_STREAM_INDEX;
        pkt->dts = ts;
        pkt->pts = ts;

        av_interleaved_write_frame(recording_fmt_ctx, pkt);

        av_packet_free(&pkt);
    }
}

int vpi_decode_is_recording()
{
	return (recording_fmt_ctx != 0);
}

int vpi_decode_record(const char *filename)
{
    int r = avformat_alloc_output_context2(&recording_fmt_ctx, 0, 0, filename);
    if (r < 0) {
		vpilog("Failed to allocate output context for recording\n");
        goto exit;
    }

	recording_vstr = avformat_new_stream(recording_fmt_ctx, 0);
    if (!recording_vstr) {
		vpilog("Failed to allocate video stream for recording\n");
        goto exit_and_free_context;
    }

    recording_vstr->id = VIDEO_STREAM_INDEX;
    recording_vstr->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    recording_vstr->codecpar->width = 854;
    recording_vstr->codecpar->height = 480;
    recording_vstr->codecpar->format = AV_PIX_FMT_YUV420P;
    recording_vstr->time_base = (AVRational) {1, 60};
    recording_vstr->codecpar->codec_id = AV_CODEC_ID_H264;

	recording_vstr->codecpar->extradata = av_malloc(200);
	recording_vstr->codecpar->extradata_size = vanilla_generate_h264_header(recording_vstr->codecpar->extradata, 200);

    recording_astr = avformat_new_stream(recording_fmt_ctx, 0);
    if (!recording_astr) {
		vpilog("Failed to allocate audio stream for recording\n");
        goto exit_and_free_context;
    }

    recording_astr->id = AUDIO_STREAM_INDEX;
    recording_astr->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    recording_astr->codecpar->sample_rate = 48000;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57,24,100)
    recording_astr->codecpar->ch_layout = (AVChannelLayout) AV_CHANNEL_LAYOUT_STEREO;
#else
    recording_astr->codecpar->channel_layout = AV_CH_LAYOUT_STEREO;
#endif
    recording_astr->codecpar->format = AV_SAMPLE_FMT_S16;
    recording_astr->time_base = (AVRational) {1, 48000};
    recording_astr->codecpar->codec_id = AV_CODEC_ID_PCM_S16LE;

	r = avio_open2(&recording_fmt_ctx->pb, filename, AVIO_FLAG_WRITE, 0, 0);
    if (r < 0) {
		vpilog("Failed to open AVIO for recording\n");
        goto exit_and_free_context;
    }

    r = avformat_write_header(recording_fmt_ctx, 0);
    if (r < 0) {
		vpilog("Failed to write header for recording\n");
        goto exit_and_free_context;
    }

	vanilla_request_idr();

	gettimeofday(&recording_start, 0);

	char buf[VPI_TOAST_MAX_LEN];
	snprintf(buf, sizeof(buf), lang(VPI_LANG_RECORDING_START), filename);
	vpi_show_toast(buf);
	vpilog("Started recording to \"%s\"\n", filename);

    goto exit;

exit_and_free_context:
    avformat_free_context(recording_fmt_ctx);
    recording_fmt_ctx = 0;

exit:
	return r;
}

void vpi_decode_record_stop()
{
	if (recording_fmt_ctx) {
        int r = av_write_trailer(recording_fmt_ctx);
		if (r < 0) {
			vpilog("Failed to write trailer for recording\n");
		}

        avio_closep(&recording_fmt_ctx->pb);

        avformat_free_context(recording_fmt_ctx);
        recording_fmt_ctx = 0;

		vpilog("Finished recording\n");
		vpi_show_toast(lang(VPI_LANG_RECORDING_FINISH));
    }
}

void vpi_decode_screenshot(const char *filename)
{
	// Grab copy of filename
	vui_strncpy(screenshot_buf, filename, sizeof(screenshot_buf));
}
