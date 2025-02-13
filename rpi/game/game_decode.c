#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <pthread.h>
#include <vanilla.h>

#include "def.h"
#include "game_main.h"

AVFrame *vpi_present_frame = 0;
static AVFrame *decoding_frame;
pthread_mutex_t vpi_decoding_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t decoding_wait_cond = PTHREAD_COND_INITIALIZER;
static AVCodecContext *video_codec_ctx;
static AVPacket *video_packet;

pthread_mutex_t vpi_decode_loop_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t vpi_decode_loop_cond = PTHREAD_COND_INITIALIZER;
int vpi_decode_loop_running = 0;
uint8_t vpi_decode_ready = 0;
uint8_t vpi_decode_data[65536];
size_t vpi_decode_size = 0;

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
			fprintf(stderr, "Failed to receive frame from decoder: %i\n", err);
			ret = 0;
			break;
		} else if (!decoding_frame->data[0]) {
			fprintf(stderr, "WE GOT A NULL DATA[0] STRAIGHT FROM THE DECODER?????\n");
			abort();
		} else if ((decoding_frame->flags & AV_FRAME_FLAG_CORRUPT) != 0) {
			fprintf(stderr, "GOT A CORRUPT FRAME??????\n");
			abort();
		} else {
			pthread_mutex_lock(&vpi_decoding_mutex);

			// Swap refs from decoding_frame to present_frame
			av_frame_unref(vpi_present_frame);
			av_frame_move_ref(vpi_present_frame, decoding_frame);
			
			pthread_cond_broadcast(&decoding_wait_cond);
			pthread_mutex_unlock(&vpi_decoding_mutex);
		}
	}

	return ret;
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
		fprintf(stderr, "No decoder was available\n");
		goto exit;
	}

    video_codec_ctx = avcodec_alloc_context3(codec);
	if (!video_codec_ctx) {
		fprintf(stderr, "Failed to allocate codec context\n");
		goto free_context;
	}

    if (using_v4l2m2m) {
        ffmpeg_err = av_hwdevice_ctx_create(&video_codec_ctx->hw_device_ctx, AV_HWDEVICE_TYPE_DRM, "/dev/dri/card0", NULL, 0);
        if (ffmpeg_err < 0) {
            fprintf(stderr, "Failed to create hwdevice context: %s (%i)\n", av_err2str(ffmpeg_err), ffmpeg_err);
            goto free_context;
        }

        // MAKE SURE WE GET DRM PRIME FRAMES BY OVERRIDING THE GET_FORMAT FUNCTION
        video_codec_ctx->get_format = get_format;
    }

	ffmpeg_err = avcodec_open2(video_codec_ctx, codec, NULL);
    if (ffmpeg_err < 0) {
		fprintf(stderr, "Failed to open decoder: %i\n", ffmpeg_err);
        goto free_context;
	}

	decoding_frame = av_frame_alloc();
    if (!decoding_frame) {
        fprintf(stderr, "Failed to allocate AVFrame\n");
        goto free_context;
    }

	vpi_present_frame = av_frame_alloc();
	if (!vpi_present_frame) {
		fprintf(stderr, "Failed to allocate AVFrame\n");
		goto free_decode_frame;
	}

	video_packet = av_packet_alloc();
	if (!video_packet) {
		fprintf(stderr, "Failed to allocate AVPacket\n");
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

        int err = avcodec_send_packet(video_codec_ctx, video_packet);

		pthread_mutex_unlock(&vpi_decode_loop_mutex);

        if (err < 0) {
            fprintf(stderr, "Failed to send packet to decoder: %s (%i)\n", av_err2str(err), err);
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