#define SCREEN_WIDTH    854
#define SCREEN_HEIGHT   480

#include <arpa/inet.h>

#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <vanilla.h>
#include <unistd.h>

#include "drm.h"

AVFrame *present_frame;
AVFrame *decoding_frame;
pthread_mutex_t decoding_mutex;
pthread_cond_t decoding_wait_cond;
AVCodecContext *video_codec_ctx;
AVCodecParserContext *video_parser;
AVPacket *video_packet;
int running = 0;

// HACK: Easy macro to test between desktop and RPi (even though ARM doesn't necessarily mean RPi)
#ifdef __arm__
#define RASPBERRY_PI
#endif

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

int decode(const void *data, size_t size)
{
	int err;

    // Parse this data for packets
	err = av_packet_from_data(video_packet, (uint8_t *) data, size);
	if (err < 0) {
		fprintf(stderr, "Failed to initialize AVPacket from data: %s (%i)\n", av_err2str(err), err);
		return 0;
	}

	// Send packet to decoder
	err = avcodec_send_packet(video_codec_ctx, video_packet);
	if (err < 0) {
		fprintf(stderr, "Failed to send packet to decoder: %s (%i)\n", av_err2str(err), err);
		return 1;
	}

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
			pthread_mutex_lock(&decoding_mutex);

			// Swap refs from decoding_frame to present_frame
			av_frame_unref(present_frame);
			av_frame_move_ref(present_frame, decoding_frame);
			
			pthread_cond_broadcast(&decoding_wait_cond);
			pthread_mutex_unlock(&decoding_mutex);
		}
	}

	return ret;
}

int run_backend(void *data)
{
	vanilla_event_t event;

	while (vanilla_wait_event(&event)) {
		if (event.type == VANILLA_EVENT_VIDEO) {
			decode(event.data, event.size);
		}
	}

	return 0;
}

void logger(const char *s, va_list args)
{
	vfprintf(stderr, s, args);
}

int display_loop(void *data)
{
	vanilla_drm_ctx_t *drm_ctx = (vanilla_drm_ctx_t *) data;

	Uint32 start_ticks = SDL_GetTicks();

	#define TICK_MAX 30
	Uint32 tick_deltas[TICK_MAX];
	size_t tick_delta_index = 0;

	AVFrame *frame = av_frame_alloc();

	pthread_mutex_lock(&decoding_mutex);

	while (running) {
		while (running && !present_frame->data[0]) {
			pthread_cond_wait(&decoding_wait_cond, &decoding_mutex);
		}

		if (!running) {
			break;
		}

		// If a frame is available, present it here
		av_frame_unref(frame);
		av_frame_move_ref(frame, present_frame);
		
		pthread_mutex_unlock(&decoding_mutex);

		display_drm(drm_ctx, frame);

		// FPS counter stuff
		Uint32 now = SDL_GetTicks();
		tick_deltas[tick_delta_index] = (now - start_ticks);
		start_ticks = now;
		tick_delta_index++;

		if (tick_delta_index == TICK_MAX) {
			Uint32 total = 0;
			for (int i = 0; i < TICK_MAX; i++) {
				total += tick_deltas[i];
			}

			fprintf(stderr, "AVERAGE FPS: %.2f\n", 1000.0f / (total / (float) TICK_MAX));

			tick_delta_index = 0;
		}
	
		pthread_mutex_lock(&decoding_mutex);
	}

	pthread_mutex_unlock(&decoding_mutex);

	av_frame_free(&frame);

	return 0;
}

SDL_GameController *find_valid_controller()
{
	for (int i = 0; i < SDL_NumJoysticks(); i++) {
		SDL_GameController *c = SDL_GameControllerOpen(i);
		if (c) {
			return c;
		}
	}

	return NULL;
}

void sigint_handler(int signum)
{
	set_tty(KD_TEXT);

	printf("Received SIGINT...\n");
	running = 0;

	SDL_QuitEvent ev;
	ev.type = SDL_QUIT;
	ev.timestamp = SDL_GetTicks();
	SDL_PushEvent((SDL_Event *) &ev);
}

int main(int argc, const char **argv)
{
	vanilla_drm_ctx_t drm_ctx;
	if (!initialize_drm(&drm_ctx)) {
		fprintf(stderr, "Failed to find DRM output\n");
		return 1;
	}

	// Initialize SDL2 for audio and input
	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
		fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
		return 1;
	}

	// Initialize FFmpeg
#ifdef RASPBERRY_PI
	const AVCodec *codec = avcodec_find_decoder_by_name("h264_v4l2m2m");
#else
	const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
#endif
	if (!codec) {
		fprintf(stderr, "No decoder was available\n");
		return 1;
	}

    video_codec_ctx = avcodec_alloc_context3(codec);
	if (!video_codec_ctx) {
		fprintf(stderr, "Failed to allocate codec context\n");
		return 1;
	}

	int ffmpeg_err;

	ffmpeg_err = av_hwdevice_ctx_create(&video_codec_ctx->hw_device_ctx, AV_HWDEVICE_TYPE_DRM, "/dev/dri/card0", NULL, 0);
	if (ffmpeg_err < 0) {
		fprintf(stderr, "Failed to create hwdevice context: %s (%i)\n", av_err2str(ffmpeg_err), ffmpeg_err);
		return 1;
	}

	// MAKE SURE WE GET DRM PRIME FRAMES BY OVERRIDING THE GET_FORMAT FUNCTION
	video_codec_ctx->get_format = get_format;

	ffmpeg_err = avcodec_open2(video_codec_ctx, codec, NULL);
    if (ffmpeg_err < 0) {
		fprintf(stderr, "Failed to open decoder: %i\n", ffmpeg_err);
		return 1;
	}

	decoding_frame = av_frame_alloc();
	present_frame = av_frame_alloc();
	if (!decoding_frame || !present_frame) {
		fprintf(stderr, "Failed to allocate AVFrame\n");
		return 1;
	}

	video_packet = av_packet_alloc();
	if (!video_packet) {
		fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
		return 1;
	}

	video_parser = av_parser_init(codec->id);
	if (!video_parser) {
        fprintf(stderr, "Failed to create codec parser\n");
        exit(1);
    }

	// Install logging debugger
	vanilla_install_logger(logger);

	// Start Vanilla
#ifdef RASPBERRY_PI
	vanilla_start(0);
#else
	vanilla_start(ntohl(inet_addr("127.0.0.1")));
#endif

	// Create variable for background threads to run
	running = 1;
	signal(SIGINT, sigint_handler);

	pthread_mutex_init(&decoding_mutex, NULL);
	pthread_cond_init(&decoding_wait_cond, NULL);

	// Start background threads
	SDL_Thread *backend_thread = SDL_CreateThread(run_backend, "vanilla-pi-backend", NULL);
	SDL_Thread *display_thread = SDL_CreateThread(display_loop, "vanilla-pi-display", &drm_ctx);

	SDL_GameController *controller = find_valid_controller();

	while (running) {
		SDL_Event event;
		if (SDL_WaitEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				// Exit loop
				running = 0;
				break;
			case SDL_CONTROLLERDEVICEADDED:
				// Attempt to find controller if one doesn't exist already
				if (!controller) {
					controller = find_valid_controller();
				}
				break;
			case SDL_CONTROLLERDEVICEREMOVED:
				// Handle current controller being removed
				if (controller && event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))) {
					SDL_GameControllerClose(controller);
					controller = find_valid_controller();
				}
				break;
			case SDL_CONTROLLERBUTTONDOWN:
			case SDL_CONTROLLERBUTTONUP:
				printf("Button: %i - Pressed: %i\n", event.cbutton.button, event.type == SDL_CONTROLLERBUTTONDOWN);
				break;
			/*case SDL_CONTROLLERBUTTONDOWN:
			case SDL_CONTROLLERBUTTONUP:
				if (m_controller && event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(m_controller))) {
					int vanilla_btn = g_buttonMap[event.cbutton.button];
					if (vanilla_btn != -1) {
						emit buttonStateChanged(vanilla_btn, event.type == SDL_CONTROLLERBUTTONDOWN ? INT16_MAX : 0);
					}
				}
				break;
			case SDL_CONTROLLERAXISMOTION:
				if (m_controller && event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(m_controller))) {
					int vanilla_axis = g_axisMap[event.caxis.axis];
					Sint16 axis_value = event.caxis.value;
					if (vanilla_axis != -1) {
						emit buttonStateChanged(vanilla_axis, axis_value);
					}
				}
				break;
			case SDL_CONTROLLERSENSORUPDATE:
				if (event.csensor.sensor == SDL_SENSOR_ACCEL) {
					emit buttonStateChanged(VANILLA_SENSOR_ACCEL_X, packFloat(event.csensor.data[0]));
					emit buttonStateChanged(VANILLA_SENSOR_ACCEL_Y, packFloat(event.csensor.data[1]));
					emit buttonStateChanged(VANILLA_SENSOR_ACCEL_Z, packFloat(event.csensor.data[2]));
				} else if (event.csensor.sensor == SDL_SENSOR_GYRO) {
					emit buttonStateChanged(VANILLA_SENSOR_GYRO_PITCH, packFloat(event.csensor.data[0]));
					emit buttonStateChanged(VANILLA_SENSOR_GYRO_YAW, packFloat(event.csensor.data[1]));
					emit buttonStateChanged(VANILLA_SENSOR_GYRO_ROLL, packFloat(event.csensor.data[2]));
				}
				break;*/
			}
    
			/*if (m_controller) {
				uint16_t amount = m_vibrate ? 0xFFFF : 0;
				SDL_GameControllerRumble(m_controller, amount, amount, 0);
			}*/
		}
	}

	// Terminate background threads and wait for them to end gracefully
	vanilla_stop();

	pthread_mutex_lock(&decoding_mutex);
	running = 0;
	pthread_cond_broadcast(&decoding_wait_cond);
	pthread_mutex_unlock(&decoding_mutex);

	SDL_WaitThread(backend_thread, NULL);
	SDL_WaitThread(display_thread, NULL);

	pthread_cond_destroy(&decoding_wait_cond);
	pthread_mutex_destroy(&decoding_mutex);

	av_parser_close(video_parser);
	av_frame_free(&present_frame);
	av_frame_free(&decoding_frame);
	av_packet_free(&video_packet);
    avcodec_free_context(&video_codec_ctx);

	SDL_Quit();

	free_drm(&drm_ctx);

	return 0;
}