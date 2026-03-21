#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
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
    if (stream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO && stream->codecpar->codec_id != AV_CODEC_ID_H264) {
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
        err = avcodec_send_packet(s.codec_ctx, s.pkt);
        if (err < 0) {
            fprintf(stderr, "avcodec_send_packet: %i\n", err);
            break;
        }
        printf("    Sent: %li\n", s.pkt->pts);
        usleep(17000);

        while ((err = avcodec_receive_frame(s.codec_ctx, s.frame)) >= 0) {
            printf("Received: %li\n", s.frame->pts);
        }



        if (err < 0 && err != AVERROR(EAGAIN)) {
            fprintf(stderr, "avcodec_receive_frame: %i\n", err);
            break;
        }
    }

    vpi_decode_exit(&s);

    return ret;
}
