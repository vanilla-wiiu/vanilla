#include "audiohandler.h"


AudioHandler::AudioHandler(QObject *parent) : QObject(parent)
{
    QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    
    m_sink = nullptr;
    m_sinkDevice = nullptr;
    
    if (!dev.isNull()) {
        QAudioFormat fmt;
        fmt.setChannelConfig(QAudioFormat::ChannelConfigStereo);
        fmt.setSampleFormat(QAudioFormat::SampleFormat::Int16);
        fmt.setSampleRate(48000);

        m_sink = new QAudioSink(dev, fmt, this);
    }
}

void AudioHandler::run()
{
    if (m_sink) {
        m_sinkDevice = m_sink->start();
    }
}

void AudioHandler::close()
{
    if (m_sink) {
        m_sink->stop();
        m_sinkDevice = nullptr;
    }
}

void AudioHandler::write(const QByteArray &data)
{
    if (m_sinkDevice) {
        m_sinkDevice->write(data);
    }
}

void AudioHandler::setVolume(qreal vol)
{
    if (m_sink) {
        m_sink->setVolume(vol);
    }
}