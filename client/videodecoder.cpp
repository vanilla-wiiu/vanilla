#include "videodecoder.h"

#include <QImage>

VideoDecoder::VideoDecoder(QObject *parent) : QObject(parent)
{
    m_packet = av_packet_alloc();
    m_frame = av_frame_alloc();
    
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    m_codecCtx = avcodec_alloc_context3(codec);
    avcodec_open2(m_codecCtx, codec, NULL);

    m_swsCtx = nullptr;
}

VideoDecoder::~VideoDecoder()
{
    sws_freeContext(m_swsCtx);
    avcodec_free_context(&m_codecCtx);
    av_frame_free(&m_frame);
    av_packet_free(&m_packet);
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
        // Got a frame! Wrap it up in a QImage and send it out
        if (m_swsCtx && (m_swsWidth != m_frame->width || m_swsHeight != m_frame->height || m_swsFormat != m_frame->format)) {
            sws_freeContext(m_swsCtx);
            m_swsCtx = nullptr;
        }

        if (!m_swsCtx) {
            m_swsWidth = m_frame->width;
            m_swsHeight = m_frame->height;
            m_swsFormat = (AVPixelFormat) m_frame->format;
            m_swsCtx = sws_getContext(m_swsWidth, m_swsHeight, m_swsFormat,
                                      m_swsWidth, m_swsHeight, AV_PIX_FMT_RGB24,
                                      SWS_FAST_BILINEAR, NULL, NULL, NULL);
        }

        QImage image(m_swsWidth, m_swsHeight, QImage::Format_RGB888);
        uint8_t *dst_slice = image.bits();
        int dst_stride = image.bytesPerLine();
        sws_scale(m_swsCtx, m_frame->data, m_frame->linesize, 0, m_frame->height, &dst_slice, &dst_stride);

        emit frameReady(image);

        av_frame_unref(m_frame);
    }
}