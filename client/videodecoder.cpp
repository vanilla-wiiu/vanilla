#include "videodecoder.h"

#include <QImage>

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

VideoDecoder::VideoDecoder(QObject *parent) : QObject(parent)
{
    m_packet = av_packet_alloc();
    m_frame = av_frame_alloc();
    
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    m_codecCtx = avcodec_alloc_context3(codec);
    avcodec_open2(m_codecCtx, codec, NULL);

    m_filterGraph = avfilter_graph_alloc();

    char args[512];
    snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=30/1:pixel_aspect=1/1", 854, 480, AV_PIX_FMT_YUV420P);
    avfilter_graph_create_filter(&m_buffersrcCtx, avfilter_get_by_name("buffer"), "in", args, NULL, m_filterGraph);

    avfilter_graph_create_filter(&m_buffersinkCtx, avfilter_get_by_name("buffersink"), "out", NULL, NULL, m_filterGraph);

    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_RGB24, AV_PIX_FMT_NONE};
    av_opt_set_int_list(m_buffersinkCtx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    
    avfilter_link(m_buffersrcCtx, 0, m_buffersinkCtx, 0);
    avfilter_graph_config(m_filterGraph, 0);
}

VideoDecoder::~VideoDecoder()
{
    avfilter_graph_free(&m_filterGraph);
    avcodec_free_context(&m_codecCtx);
    av_frame_free(&m_frame);
    av_packet_free(&m_packet);
}

void cleanupFrame(void *v)
{
    AVFrame *f = (AVFrame *) v;
    av_frame_free(&f);
}

void VideoDecoder::sendPacket(const QByteArray &data)
{
    int ret;

    // Copy data into buffer that FFmpeg will take ownership of
    uint8_t *buffer = (uint8_t *) av_malloc(data.size());
    memcpy(buffer, data.data(), data.size());

    // Create AVPacket from this data
    ret = av_packet_from_data(m_packet, buffer, data.size());
    if (ret < 0) {
        fprintf(stderr, "Failed to initialize packet from data: %i\n", ret);
        av_free(buffer);
        return;
    }

    // Send packet to decoder
    ret = avcodec_send_packet(m_codecCtx, m_packet);
    av_packet_unref(m_packet);
    if (ret < 0) {
        fprintf(stderr, "Failed to send packet to decoder: %i\n", ret);
        return;
    }

    // Retrieve frame from decoder
    ret = avcodec_receive_frame(m_codecCtx, m_frame);
    if (ret == AVERROR(EAGAIN)) {
        // Decoder wants another packet before it can output a frame. Silently exit.
    } else if (ret < 0) {
        fprintf(stderr, "Failed to receive frame from decoder: %i\n", ret);
    } else {
        ret = av_buffersrc_add_frame_flags(m_buffersrcCtx, m_frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        av_frame_unref(m_frame);
        if (ret < 0) {
            fprintf(stderr, "Failed to add frame to buffersrc: %i\n", ret);
            return;
        }

        AVFrame *filtered = av_frame_alloc();
        ret = av_buffersink_get_frame(m_buffersinkCtx, filtered);
        if (ret < 0) {
            fprintf(stderr, "Failed to get frame from buffersink: %i\n", ret);
            av_frame_free(&filtered);
            return;
        }

        QImage image(filtered->data[0], filtered->width, filtered->height, filtered->linesize[0], QImage::Format_RGB888, cleanupFrame, filtered);
        emit frameReady(image);
    }
}