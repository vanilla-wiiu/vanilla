#ifndef VIDEO_DECODER_H
#define VIDEO_DECODER_H

#include <QObject>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}

class VideoDecoder : public QObject
{
    Q_OBJECT
public:
    VideoDecoder(QObject *parent = nullptr);
    virtual ~VideoDecoder() override;

signals:
    void frameReady(const QImage &image);

public slots:
    void sendPacket(const QByteArray &data);

private:
    AVCodecContext *m_codecCtx;
    AVPacket *m_packet;
    AVFrame *m_frame;
    
    AVFilterGraph *m_filterGraph;
    AVFilterContext *m_buffersrcCtx;
    AVFilterContext *m_buffersinkCtx;

    QByteArray m_currentPacket;

};

#endif // VIDEO_DECODER_H
