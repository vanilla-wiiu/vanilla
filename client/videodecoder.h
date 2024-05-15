#ifndef VIDEO_DECODER_H
#define VIDEO_DECODER_H

#include <QObject>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
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
    SwsContext *m_swsCtx;
    int m_swsWidth;
    int m_swsHeight;
    AVPixelFormat m_swsFormat;

    QByteArray m_currentPacket;

};

#endif // VIDEO_DECODER_H
