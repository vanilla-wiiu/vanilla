#ifndef VIDEO_DECODER_H
#define VIDEO_DECODER_H

#include <QObject>

extern "C" {
#include <libavformat/avformat.h>
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
    void recordingError(int err);
    void recordingFinished(const QString &filename);
    void requestIDR();

public slots:
    void sendPacket(const QByteArray &data);
    void sendAudio(const QByteArray &data);
    void enableRecording(bool e);
    void startRecording();
    void stopRecording();

private:
    int64_t getCurrentTimestamp(AVRational timebase);
    
    AVCodecContext *m_codecCtx;
    AVPacket *m_packet;
    AVFrame *m_frame;
    
    AVFilterGraph *m_filterGraph;
    AVFilterContext *m_buffersrcCtx;
    AVFilterContext *m_buffersinkCtx;

    AVFormatContext *m_recordingCtx;
    AVStream *m_videoStream;
    AVStream *m_audioStream;
    QString m_recordingFilename;
    int64_t m_recordingStartTime;

};

#endif // VIDEO_DECODER_H
