#ifndef AUDIOHANDLER_H
#define AUDIOHANDLER_H

#include <QAudioSink>
#include <QMediaDevices>
#include <QObject>

class AudioHandler : public QObject
{
    Q_OBJECT
public:
    AudioHandler(QObject *parent = nullptr);

public slots:
    void run();

    void close();

    void write(const QByteArray &data);

private:
    //QMediaDevices *m_mediaDevices;
    QAudioSink *m_sink;
    QIODevice *m_sinkDevice;

};

#endif // AUDIOHANDLER_H