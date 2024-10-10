#define SCREEN_WIDTH    854
#define SCREEN_HEIGHT   480

#include <arpa/inet.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <vanilla.h>

AVFrame *present_frame;
AVFrame *filtering_frame;
AVFrame *decoding_frame;
SDL_mutex *decoding_mutex;
int decoding_ready = 0;
AVCodecContext *video_codec_ctx;
AVPacket *video_packet;
AVFilterGraph *m_filterGraph;
AVFilterContext *m_buffersrcCtx;
AVFilterContext *m_buffersinkCtx;

int decode_frame(const void *data, size_t size)
{
	int ret;

    // Copy data into buffer that FFmpeg will take ownership of
    uint8_t *buffer = (uint8_t *) av_malloc(size);
    memcpy(buffer, data, size);

    // Create AVPacket from this data
    ret = av_packet_from_data(video_packet, buffer, size);
    if (ret < 0) {
        fprintf(stderr, "Failed to initialize packet from data: %i\n", ret);
        av_free(buffer);
        return 1;
    }

    // Send packet to decoder
    ret = avcodec_send_packet(video_codec_ctx, video_packet);
    av_packet_unref(video_packet);
    if (ret < 0) {
        fprintf(stderr, "Failed to send packet to decoder: %i\n", ret);
        return 1;
    }

    // Retrieve frame from decoder
    ret = avcodec_receive_frame(video_codec_ctx, decoding_frame);
    if (ret == AVERROR(EAGAIN)) {
        // Decoder wants another packet before it can output a frame. Silently exit.
    } else if (ret < 0) {
        fprintf(stderr, "Failed to receive frame from decoder: %i\n", ret);
    } else {
        ret = av_buffersrc_add_frame_flags(m_buffersrcCtx, decoding_frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        av_frame_unref(decoding_frame);
        if (ret < 0) {
            fprintf(stderr, "Failed to add frame to buffersrc: %i\n", ret);
            return 1;
        }

        ret = av_buffersink_get_frame(m_buffersinkCtx, filtering_frame);
        if (ret < 0) {
            fprintf(stderr, "Failed to get frame from buffersink: %i\n", ret);
            av_frame_free(&filtering_frame);
            return 1;
        }

		SDL_LockMutex(decoding_mutex);
		
		// Swap frames
		AVFrame *tmp = filtering_frame;
		filtering_frame = present_frame;
		present_frame = tmp;

		// Un-ref frame
		av_frame_unref(filtering_frame);

		// Signal we have a frame
		decoding_ready = 1;
		
		SDL_UnlockMutex(decoding_mutex);
    }
}

void event_handler(void *context, int event_type, const char *data, size_t data_size)
{
	if (event_type == VANILLA_EVENT_VIDEO) {
		// Decode the frame!!!!!!!!
		decode_frame(data, data_size);
	}
}

int run_backend(void *data)
{
	vanilla_start_udp(event_handler, NULL, inet_addr("127.0.0.1"));
	return 0;
}

void logger(const char *s, va_list args)
{
	vfprintf(stderr, s, args);
}

int main(int argc, const char **argv)
{
	// Initialize SDL2
	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
	
	SDL_Window* window = NULL;
	SDL_Surface* screenSurface = NULL;
	if (SDL_Init(SDL_INIT_VIDEO/* | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER*/) < 0) {
		fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
		return 1;
	}

	window = SDL_CreateWindow(
				"Vanilla Pi",
				SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
				SCREEN_WIDTH, SCREEN_HEIGHT,
				SDL_WINDOW_SHOWN
			);
	if (window == NULL) {
		fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
		return 1;
	}

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer) {
		fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
		return 1;
	}

	// Initialize FFmpeg
	const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (!codec) {
		fprintf(stderr, "No decoder was available\n");
		return 1;
	}

    AVCodecContext *video_codec_ctx = avcodec_alloc_context3(codec);
	if (!video_codec_ctx) {
		fprintf(stderr, "Failed to allocate codec context\n");
		return 1;
	}

	int ffmpeg_err = avcodec_open2(video_codec_ctx, codec, NULL);
    if (ffmpeg_err < 0) {
		fprintf(stderr, "Failed to open decoder: %i\n", ffmpeg_err);
		return 1;
	}

	video_packet = av_packet_alloc();
	if (!video_packet) {
		fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
		return 1;
	}

	m_filterGraph = avfilter_graph_alloc();

    char args[512];
    snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=30/1:pixel_aspect=1/1", 854, 480, AV_PIX_FMT_YUV420P);
    avfilter_graph_create_filter(&m_buffersrcCtx, avfilter_get_by_name("buffer"), "in", args, NULL, m_filterGraph);

    avfilter_graph_create_filter(&m_buffersinkCtx, avfilter_get_by_name("buffersink"), "out", NULL, NULL, m_filterGraph);

    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_RGB24, AV_PIX_FMT_NONE};
    av_opt_set_int_list(m_buffersinkCtx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    
    avfilter_link(m_buffersrcCtx, 0, m_buffersinkCtx, 0);
    avfilter_graph_config(m_filterGraph, 0);

	// Install logging debugger
	vanilla_install_logger(logger);

	// Launch backend on second thread
	SDL_Thread *backend_thread = SDL_CreateThread(run_backend, "Backend", NULL);

	// Create main video display texture
	SDL_Texture *main_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);

	int delay = 16;

	while (1) {
		SDL_Event event;
		if (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				vanilla_stop();
				break;
			}
		}

		// If a frame is available, present it here
		SDL_LockMutex(decoding_mutex);
		if (decoding_ready) {
			SDL_UpdateTexture(main_texture, NULL, present_frame->data[0], present_frame->linesize[0]);
		}
		SDL_UnlockMutex(decoding_mutex);

		// int height = 64;
		// SDL_SetRenderDrawColor(renderer, 0xFF, 0x00, 0xFF, 0xFF);
		// SDL_RenderFillRect(renderer, NULL);
		// SDL_Rect r = {0, 240 - height/2, hasFrame, height};
		// SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
		// SDL_RenderFillRect(renderer, &r);

		SDL_RenderCopy(renderer, main_texture, NULL, NULL);
		SDL_RenderPresent(renderer);
		
		SDL_Delay(16);
	}
	vanilla_stop();

	SDL_WaitThread(backend_thread, NULL);

    avfilter_graph_free(&m_filterGraph);
	av_packet_free(&video_packet);
    avcodec_free_context(&video_codec_ctx);

	SDL_DestroyTexture(main_texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}