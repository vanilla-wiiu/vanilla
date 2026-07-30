#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <unistd.h>

#include "menu/menu_game.h"

int main(int argc, const char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    const char *fn = argv[1];

    int err;
    AVFormatContext *fmt_ctx = 0;

    err = avformat_open_input(&fmt_ctx, fn, 0, 0);
    if (err < 0) {
        fprintf(stderr, "avformat_open_input: %i\n", err);
        return 1;
    }

    err = avformat_find_stream_info(fmt_ctx, NULL);
    if (err < 0) {
        fprintf(stderr, "avformat_find_stream_info: %i\n", err);
        return 1;
    }

    if (fmt_ctx->nb_streams != 1) {
        fprintf(stderr, "Invalid stream count\n", err);
        return 1;
    }

    AVStream *stream = fmt_ctx->streams[0];

    if (stream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
        stream->codecpar->codec_id != AV_CODEC_ID_H264) {
        fprintf(stderr, "Invalid stream 0\n", err);
        return 1;
    }

    vpi_decode_state_t s;
    memset(&s, 0, sizeof(s));

    err = vpi_decode_init(&s);
    if (err < 0) {
        return 1;
    }

    int ret = 1;

    int64_t pts = 0;

    while (1) {
        err = av_read_frame(fmt_ctx, s.pkt);
        if (err < 0) {
            if (err == AVERROR_EOF) {
                ret = 0;
            } else {
                fprintf(stderr, "av_read_frame: %i\n", err);
            }
            break;
        }

        if (s.pkt->stream_index != 0) {
            continue;
        }

        // Retrieve frame from decoder
        s.pkt->pts = pts++;

        int64_t decode_start = av_gettime_relative();

        err = avcodec_send_packet(s.codec_ctx, s.pkt);
        if (err < 0) {
            fprintf(stderr, "avcodec_send_packet: %i\n", err);
            break;
        }

        printf("    Sent: %li\n", s.pkt->pts);

        // Raspberry Pi decoder may return EAGAIN before the asynchronously decoded frame is ready. Retry briefly without submitting another packet to make sure the queue is drained.
        int received_any = 0;
        int64_t deadline = av_gettime_relative() + 100000;

        while (1) {
            err = avcodec_receive_frame(s.codec_ctx, s.frame);

            if (err >= 0) {
                int64_t decode_end = av_gettime_relative();
                int64_t decode_time_us = decode_end - decode_start;

                printf(
                    "Received: %li, total decode time: %.3f ms\n",
                    s.frame->pts,
                    decode_time_us / 1000.0
                );

                received_any = 1;
                continue;
            }

            if (err == AVERROR(EAGAIN)) {
                printf("Got EAGAIN\n");

                if (received_any) {
                    break;
                }

                if (av_gettime_relative() >= deadline) {
                    break;
                }

                av_usleep(500);
                continue;
            }

            break;
        }

        if (err < 0 &&
            err != AVERROR(EAGAIN) &&
            err != AVERROR_EOF) {
            fprintf(stderr, "avcodec_receive_frame: %i\n", err);
            break;
        }

        // usleep(17000);
    }

    vpi_decode_exit(&s);

    return ret;
}