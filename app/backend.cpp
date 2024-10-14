#include "backend.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QMessageBox>
#include <QNetworkDatagram>
#include <QtConcurrent/QtConcurrent>

#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <vanilla.h>

void Backend::vanillaEventHandler()
{
    vanilla_event_t event;

    while (vanilla_wait_event(&event)) {
        switch (event.type) {
        case VANILLA_EVENT_VIDEO:
            emit videoAvailable(QByteArray((const char *) event.data, event.size));
            break;
        case VANILLA_EVENT_AUDIO:
            emit audioAvailable(QByteArray((const char *) event.data, event.size));
            break;
        case VANILLA_EVENT_VIBRATE:
            emit vibrate(*event.data);
            break;
        }
    }
}

Backend::Backend(QObject *parent) : QObject(parent)
{
}

void Backend::init()
{
    initInternal();
}

int Backend::initInternal()
{
    emit ready();
    return 0;
}

void Backend::interrupt()
{
    vanilla_stop();
}

void Backend::requestIDR()
{
    vanilla_request_idr();
}

void Backend::sync(uint16_t code)
{
    auto watcher = new QFutureWatcher<int>();
    connect(watcher, &QFutureWatcher<int>::finished, this, &Backend::syncFutureCompleted);
    watcher->setFuture(QtConcurrent::run(&Backend::syncInternal, this, code));
}

int Backend::syncInternal(uint16_t code)
{
    return vanilla_sync(code, 0);
}

void Backend::connectToConsole()
{
    connectInternal();
    QtConcurrent::run(&Backend::vanillaEventHandler, this);
}

int Backend::connectInternal()
{
    return vanilla_start(0);
}

void Backend::updateTouch(int x, int y)
{
    vanilla_set_touch(x, y);
}

void Backend::setButton(int button, int32_t value)
{
    vanilla_set_button(button, value);
}

void Backend::setRegion(int region)
{
    vanilla_set_region(region);
}

void Backend::setBatteryStatus(int status)
{
    vanilla_set_battery_status(status);
}

void Backend::syncFutureCompleted()
{
    QFutureWatcher<int> *watcher = static_cast<QFutureWatcher<int>*>(sender());
    int r = watcher->result();
    emit syncCompleted(r == VANILLA_SUCCESS);
    watcher->deleteLater();
}

BackendPipe::BackendPipe(const QString &wirelessInterface, QObject *parent) : QObject(parent)
{
    m_process = nullptr;
    m_wirelessInterface = wirelessInterface;

    m_process = new QProcess(this);
    m_process->setReadChannel(QProcess::StandardError);
    connect(m_process, &QProcess::readyReadStandardError, this, &BackendPipe::receivedData);
    connect(m_process, &QProcess::finished, this, &BackendPipe::closed);
}

BackendPipe::~BackendPipe()
{
    quit();
}

void BackendPipe::start()
{
    m_process->start(QStringLiteral("pkexec"), {pipeProcessFilename(), m_wirelessInterface});
}

QString BackendPipe::pipeProcessFilename()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("vanilla-pipe"));
}

void BackendPipe::receivedData()
{
    while (m_process->canReadLine()) {
        QByteArray a = m_process->readLine().trimmed();
        if (a == QByteArrayLiteral("READY")) {
            emit pipeAvailable();
        } else {
            printf("%s\n", a.constData());
        }
    }
}

void BackendPipe::quit()
{
    if (m_process) {
        m_process->write(QByteArrayLiteral("QUIT\n"));
        m_process->waitForFinished();
        m_process->deleteLater();
        m_process = nullptr;
    }
}

BackendViaInternalPipe::BackendViaInternalPipe(const QString &wirelessInterface, QObject *parent) : Backend(parent)
{
    m_wirelessInterface = wirelessInterface;
}

int BackendViaInternalPipe::initInternal()
{
    m_pipe = new BackendPipe(m_wirelessInterface, this);
    connect(m_pipe, &BackendPipe::pipeAvailable, this, &BackendViaInternalPipe::ready);
    m_pipe->start();
    return 0;
}

int BackendViaInternalPipe::syncInternal(uint16_t code)
{
    return vanilla_sync(code, QHostAddress(QHostAddress::LocalHost).toIPv4Address());
}

int BackendViaInternalPipe::connectInternal()
{
    return vanilla_start(QHostAddress(QHostAddress::LocalHost).toIPv4Address());
}

BackendViaExternalPipe::BackendViaExternalPipe(const QHostAddress &udpServer, QObject *parent) : Backend(parent)
{
    m_serverAddress = udpServer;
}

int BackendViaExternalPipe::syncInternal(uint16_t code)
{
    return vanilla_sync(code, m_serverAddress.toIPv4Address());
}

int BackendViaExternalPipe::connectInternal()
{
    return vanilla_start(m_serverAddress.toIPv4Address());
}