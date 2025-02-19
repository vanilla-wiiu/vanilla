#include "game_decode.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <sys/time.h>
#include <vanilla.h>

#include "def.h"
#include "game_main.h"
#include "lang.h"
#include "platform.h"
#include "ui/ui_util.h"

pthread_mutex_t vpi_decoding_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t decoding_wait_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t vpi_decode_loop_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t vpi_decode_loop_cond = PTHREAD_COND_INITIALIZER;
int vpi_decode_loop_running = 0;
uint8_t vpi_decode_ready = 0;
uint8_t vpi_decode_data[65536];
size_t vpi_decode_size = 0;

AVFrame *vpi_present_frame = 0;
static AVFrame *decoding_frame;
static AVCodecContext *video_codec_ctx;
static AVPacket *video_packet;

static pthread_mutex_t screenshot_mutex = PTHREAD_MUTEX_INITIALIZER;
static char screenshot_buf[4096] = {0};

static pthread_mutex_t recording_mutex = PTHREAD_MUTEX_INITIALIZER;
static AVFormatContext *recording_fmt_ctx = 0;
static AVStream *recording_vstr;
static AVStream *recording_astr;
static struct timeval recording_start;

static const int VIDEO_STREAM_INDEX = 0;
static const int AUDIO_STREAM_INDEX = 1;

enum AVPixelFormat get_format(AVCodecContext* ctx, const enum AVPixelFormat *pix_fmt)
{
    while (*pix_fmt != AV_PIX_FMT_NONE) {
        if (*pix_fmt == AV_PIX_FMT_DRM_PRIME) {
            return AV_PIX_FMT_DRM_PRIME;
		}
        pix_fmt++;
    }

    return AV_PIX_FMT_NONE;
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
	ocodec_ctx->frame_num = 1;

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

int64_t get_recording_timestamp(AVRational timebase)
{
	struct timeval tv;
	gettimeofday(&tv, 0);

	int64_t us = (tv.tv_sec - recording_start.tv_sec) * 1000000 + (tv.tv_usec - recording_start.tv_usec);
    return av_rescale_q(us, (AVRational) {1, 1000000}, timebase);
}

void vpi_decode_send_audio(const void *data, size_t size)
{
	pthread_mutex_lock(&recording_mutex);

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

	pthread_mutex_unlock(&recording_mutex);
}

int vpi_decode_is_recording()
{
	int i;
	pthread_mutex_lock(&recording_mutex);
	i = (recording_fmt_ctx != 0);
	pthread_mutex_unlock(&recording_mutex);
	return i;
}

int vpi_decode_record(const char *filename)
{
	pthread_mutex_lock(&recording_mutex);

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
	pthread_mutex_unlock(&recording_mutex);

	return r;
}

void vpi_decode_record_stop()
{
	pthread_mutex_lock(&recording_mutex);

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

	pthread_mutex_unlock(&recording_mutex);
}

int decode()
{
	int err;

	int ret = 1;

	// Retrieve frame from decoder
	while (1) {
		err = avcodec_receive_frame(video_codec_ctx, decoding_frame);
		if (err == AVERROR(EAGAIN)) {
			// Decoder wants another packet before it can output a frame. Silently exit.
			break;
		} else if (err < 0) {
			vpilog("Failed to receive frame from decoder: %i\n", err);
			ret = 0;
			break;
		} else if (!decoding_frame->data[0]) {
			vpilog("WE GOT A NULL DATA[0] STRAIGHT FROM THE DECODER?????\n");
			abort();
		} else if ((decoding_frame->flags & AV_FRAME_FLAG_CORRUPT) != 0) {
			vpilog("GOT A CORRUPT FRAME??????\n");
			abort();
		} else {
			pthread_mutex_lock(&vpi_decoding_mutex);

			// Swap refs from decoding_frame to present_frame
			av_frame_unref(vpi_present_frame);
			av_frame_move_ref(vpi_present_frame, decoding_frame);

			pthread_mutex_lock(&screenshot_mutex);
			if (screenshot_buf[0] != 0) {
				// Dump this frame into file
				dump_frame_to_file(vpi_present_frame, screenshot_buf);
				screenshot_buf[0] = 0;
			}
			pthread_mutex_unlock(&screenshot_mutex);
			
			pthread_cond_broadcast(&decoding_wait_cond);
			pthread_mutex_unlock(&vpi_decoding_mutex);
		}
	}

	return ret;
}

void vpi_decode_screenshot(const char *filename)
{
	// Grab copy of filename
	pthread_mutex_lock(&screenshot_mutex);
	vui_strncpy(screenshot_buf, filename, sizeof(screenshot_buf));
	pthread_mutex_unlock(&screenshot_mutex);
}

void *vpi_decode_loop(void *)
{
    int ret = VANILLA_PI_ERR_DECODER;
    int ffmpeg_err;

    // Initialize FFmpeg
    int using_v4l2m2m = 0;
	const AVCodec *codec = 0;

    if (using_v4l2m2m) {
        codec = avcodec_find_decoder_by_name("h264_v4l2m2m");
    }
    if (!codec) {
        codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        using_v4l2m2m = 0;
    }
	if (!codec) {
		vpilog("No decoder was available\n");
		goto exit;
	}

    video_codec_ctx = avcodec_alloc_context3(codec);
	if (!video_codec_ctx) {
		vpilog("Failed to allocate codec context\n");
		goto free_context;
	}

    if (using_v4l2m2m) {
        ffmpeg_err = av_hwdevice_ctx_create(&video_codec_ctx->hw_device_ctx, AV_HWDEVICE_TYPE_DRM, "/dev/dri/card0", NULL, 0);
        if (ffmpeg_err < 0) {
            vpilog("Failed to create hwdevice context: %s (%i)\n", av_err2str(ffmpeg_err), ffmpeg_err);
            goto free_context;
        }

        // MAKE SURE WE GET DRM PRIME FRAMES BY OVERRIDING THE GET_FORMAT FUNCTION
        video_codec_ctx->get_format = get_format;
    }

	ffmpeg_err = avcodec_open2(video_codec_ctx, codec, NULL);
    if (ffmpeg_err < 0) {
		vpilog("Failed to open decoder: %i\n", ffmpeg_err);
        goto free_context;
	}

	decoding_frame = av_frame_alloc();
    if (!decoding_frame) {
        vpilog("Failed to allocate AVFrame\n");
        goto free_context;
    }

	vpi_present_frame = av_frame_alloc();
	if (!vpi_present_frame) {
		vpilog("Failed to allocate AVFrame\n");
		goto free_decode_frame;
	}

	video_packet = av_packet_alloc();
	if (!video_packet) {
		vpilog("Failed to allocate AVPacket\n");
		goto free_present_frame;
	}

	AVFrame *frame = av_frame_alloc();

    pthread_mutex_lock(&vpi_decode_loop_mutex);
	while (vpi_decode_loop_running) {
		while (vpi_decode_loop_running && !vpi_decode_ready) {
            pthread_cond_wait(&vpi_decode_loop_cond, &vpi_decode_loop_mutex);
		}

		if (!vpi_decode_loop_running) {
			break;
		}

        // Send packet to decoder
        video_packet->data = vpi_decode_data;
        video_packet->size = vpi_decode_size;
        vpi_decode_ready = 0;

		pthread_mutex_lock(&recording_mutex);
		if (recording_fmt_ctx) {
			video_packet->stream_index = VIDEO_STREAM_INDEX;
	
			int64_t ts = get_recording_timestamp(recording_vstr->time_base);
	
			video_packet->dts = ts;
			video_packet->pts = ts;

			av_interleaved_write_frame(recording_fmt_ctx, video_packet);
			
			// av_interleaved_write_frame() eventually calls av_packet_unref(),
			// so we must put the references back in
			video_packet->data = vpi_decode_data;
			video_packet->size = vpi_decode_size;
		}
		pthread_mutex_unlock(&recording_mutex);

        int err = avcodec_send_packet(video_codec_ctx, video_packet);

		pthread_mutex_unlock(&vpi_decode_loop_mutex);

        if (err < 0) {
            vpilog("Failed to send packet to decoder: %s (%i)\n", av_err2str(err), err);
            // return 0;

			vanilla_request_idr();
        } else {
            decode();
        }

		pthread_mutex_lock(&vpi_decode_loop_mutex);
	}
	pthread_mutex_unlock(&vpi_decode_loop_mutex);

    ret = VANILLA_SUCCESS;

    // TEMP: Signals to DRM thread
    // pthread_mutex_lock(&decoding_mutex);
	// running = 0;
	// pthread_cond_broadcast(&decoding_wait_cond);
	// pthread_mutex_unlock(&decoding_mutex);

	av_frame_free(&frame);

free_packet:
	av_packet_free(&video_packet);

free_present_frame:
	av_frame_free(&vpi_present_frame);
	vpi_present_frame = 0;

free_decode_frame:
	av_frame_free(&decoding_frame);

free_context:
    avcodec_free_context(&video_codec_ctx);

exit:
    if (ret != VANILLA_SUCCESS) {
        vpi_game_set_error(ret);
    }

    return 0;
}