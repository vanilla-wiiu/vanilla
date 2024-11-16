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

static int button_map[SDL_CONTROLLER_BUTTON_MAX] = {-1};
static int axis_map[SDL_CONTROLLER_AXIS_MAX] = {-1};
static int vibrate = 0;

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

	int64_t ticks = SDL_GetTicks64();

	// Send packet to decoder
	err = avcodec_send_packet(video_codec_ctx, video_packet);
	if (err < 0) {
		fprintf(stderr, "Failed to send packet to decoder: %s (%i)\n", av_err2str(err), err);
		return 0;
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

static SDL_mutex *decode_loop_mutex;
static SDL_cond *decode_loop_cond;
static int decode_loop_running = 0;
static int decode_pkt_ready = 0;
static uint8_t decode_data[65536];
static size_t decode_size = 0;
int decode_loop(void *)
{
	uint8_t our_data[65536];
	size_t our_data_size = 0;

	SDL_LockMutex(decode_loop_mutex);
	while (decode_loop_running) {
		while (!decode_pkt_ready) {
			SDL_CondWait(decode_loop_cond, decode_loop_mutex);
		}
		
		memcpy(our_data, decode_data, decode_size);
		our_data_size = decode_size;
		
		SDL_UnlockMutex(decode_loop_mutex);

		decode(our_data, decode_size);

		SDL_LockMutex(decode_loop_mutex);
	}
	SDL_UnlockMutex(decode_loop_mutex);
}

int run_backend(void *data)
{
	SDL_AudioSpec desired = {0};
	desired.freq = 48000;
	desired.format = AUDIO_S16LSB;
	desired.channels = 2;

	SDL_AudioDeviceID audio = SDL_OpenAudioDevice(NULL, 0, &desired, NULL, 0);
	if (audio) {
		SDL_PauseAudioDevice(audio, 0);
	} else {
		printf("Failed to open audio device\n");
	}

	vanilla_event_t event;

	decode_loop_mutex = SDL_CreateMutex();
	decode_loop_cond = SDL_CreateCond();
	decode_loop_running = 1;

	SDL_Thread *decode_thread = SDL_CreateThread(decode_loop, "vanilla-decode", NULL);

	while (vanilla_wait_event(&event)) {
		if (event.type == VANILLA_EVENT_VIDEO) {
			SDL_LockMutex(decode_loop_mutex);
			memcpy(decode_data, event.data, event.size);
			decode_size = event.size;
			decode_pkt_ready = 1;
			SDL_CondBroadcast(decode_loop_cond);
			SDL_UnlockMutex(decode_loop_mutex);
			// decode(event.data, event.size);
		} else if (event.type == VANILLA_EVENT_AUDIO) {
			if (audio) {
				if (SDL_QueueAudio(audio, event.data, event.size) < 0) {
					printf("Failed to queue audio\n", event.size);
				}
			}
		} else if (event.type == VANILLA_EVENT_VIBRATE) {
			vibrate = event.data[0];
		}

		vanilla_free_event(&event);
	}

	SDL_LockMutex(decode_loop_mutex);
	decode_loop_running = 0;
	SDL_CondBroadcast(decode_loop_cond);
	SDL_UnlockMutex(decode_loop_mutex);

	SDL_WaitThread(decode_thread, NULL);
	SDL_DestroyCond(decode_loop_cond);
	SDL_DestroyMutex(decode_loop_mutex);

	SDL_CloseAudioDevice(audio);

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

void init_gamepad()
{
	vibrate = 0;

    button_map[SDL_CONTROLLER_BUTTON_A] = VANILLA_BTN_A;
    button_map[SDL_CONTROLLER_BUTTON_B] = VANILLA_BTN_B;
    button_map[SDL_CONTROLLER_BUTTON_X] = VANILLA_BTN_X;
    button_map[SDL_CONTROLLER_BUTTON_Y] = VANILLA_BTN_Y;
    button_map[SDL_CONTROLLER_BUTTON_BACK] = VANILLA_BTN_MINUS;
    button_map[SDL_CONTROLLER_BUTTON_GUIDE] = VANILLA_BTN_HOME;
    button_map[SDL_CONTROLLER_BUTTON_MISC1] = VANILLA_BTN_TV;
    button_map[SDL_CONTROLLER_BUTTON_START] = VANILLA_BTN_PLUS;
    button_map[SDL_CONTROLLER_BUTTON_LEFTSTICK] = VANILLA_BTN_L3;
    button_map[SDL_CONTROLLER_BUTTON_RIGHTSTICK] = VANILLA_BTN_R3;
    button_map[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] = VANILLA_BTN_L;
    button_map[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = VANILLA_BTN_R;
    button_map[SDL_CONTROLLER_BUTTON_DPAD_UP] = VANILLA_BTN_UP;
    button_map[SDL_CONTROLLER_BUTTON_DPAD_DOWN] = VANILLA_BTN_DOWN;
    button_map[SDL_CONTROLLER_BUTTON_DPAD_LEFT] = VANILLA_BTN_LEFT;
    button_map[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] = VANILLA_BTN_RIGHT;
    axis_map[SDL_CONTROLLER_AXIS_LEFTX] = VANILLA_AXIS_L_X;
    axis_map[SDL_CONTROLLER_AXIS_LEFTY] = VANILLA_AXIS_L_Y;
    axis_map[SDL_CONTROLLER_AXIS_RIGHTX] = VANILLA_AXIS_R_X;
    axis_map[SDL_CONTROLLER_AXIS_RIGHTY] = VANILLA_AXIS_R_Y;
    axis_map[SDL_CONTROLLER_AXIS_TRIGGERLEFT] = VANILLA_BTN_ZL;
    axis_map[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] = VANILLA_BTN_ZR;
}

int32_t pack_float(float f)
{
    int32_t x;
    memcpy(&x, &f, sizeof(int32_t));
    return x;
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

	// Initialize gamepad lookup tables
	init_gamepad();

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
				if (controller && event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))) {
                    int vanilla_btn = button_map[event.cbutton.button];
                    if (vanilla_btn != -1) {
						vanilla_set_button(vanilla_btn, event.type == SDL_CONTROLLERBUTTONDOWN ? INT16_MAX : 0);
                    }
                }
				break;
			case SDL_CONTROLLERAXISMOTION:
				if (controller && event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))) {
					int vanilla_axis = axis_map[event.caxis.axis];
					Sint16 axis_value = event.caxis.value;
					if (vanilla_axis != -1) {
						vanilla_set_button(vanilla_axis, axis_value);
					}
				}
				break;
			case SDL_CONTROLLERSENSORUPDATE:
				if (event.csensor.sensor == SDL_SENSOR_ACCEL) {
					vanilla_set_button(VANILLA_SENSOR_ACCEL_X, pack_float(event.csensor.data[0]));
					vanilla_set_button(VANILLA_SENSOR_ACCEL_Y, pack_float(event.csensor.data[1]));
					vanilla_set_button(VANILLA_SENSOR_ACCEL_Z, pack_float(event.csensor.data[2]));
				} else if (event.csensor.sensor == SDL_SENSOR_GYRO) {
					vanilla_set_button(VANILLA_SENSOR_GYRO_PITCH, pack_float(event.csensor.data[0]));
					vanilla_set_button(VANILLA_SENSOR_GYRO_YAW, pack_float(event.csensor.data[1]));
					vanilla_set_button(VANILLA_SENSOR_GYRO_ROLL, pack_float(event.csensor.data[2]));
				}
				break;
			case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_ESCAPE) {
					running = 0;
				}
				break;
			}
    
			if (controller) {
				uint16_t amount = vibrate ? 0xFFFF : 0;
				SDL_GameControllerRumble(controller, amount, amount, 0);
			}
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