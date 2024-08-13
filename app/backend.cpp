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

void vanillaEventHandler(void *context, int type, const char *data, size_t dataLength)
{
    Backend *backend = static_cast<Backend*>(context);

    switch (type) {
    case VANILLA_EVENT_VIDEO:
        emit backend->videoAvailable(QByteArray(data, dataLength));
        break;
    case VANILLA_EVENT_AUDIO:
        emit backend->audioAvailable(QByteArray(data, dataLength));
        break;
    case VANILLA_EVENT_VIBRATE:
        emit backend->vibrate(*data);
        break;
    }
}

Backend::Backend(QObject *parent) : QObject(parent)
{
}

void Backend::init()
{
    emit ready();
}

BackendViaLocalRoot::BackendViaLocalRoot(const QHostAddress &udpServer, QObject *parent) : Backend(parent)
{
    m_serverAddress = udpServer;
}

void BackendViaLocalRoot::interrupt()
{
    vanilla_stop();
}

void BackendViaLocalRoot::requestIDR()
{
    vanilla_request_idr();
}

void BackendViaLocalRoot::connectToConsole()
{
    QtConcurrent::run(connectInternal, this, m_serverAddress);
}

int BackendViaLocalRoot::connectInternal(BackendViaLocalRoot *instance, const QHostAddress &server)
{
    if (server.isNull()) {
        return vanilla_start(vanillaEventHandler, instance);
    } else {
        return vanilla_start_udp(vanillaEventHandler, instance, server.toIPv4Address());
    }
}

void BackendViaLocalRoot::updateTouch(int x, int y)
{
    vanilla_set_touch(x, y);
}

void BackendViaLocalRoot::setButton(int button, int32_t value)
{
    vanilla_set_button(button, value);
}

void BackendViaLocalRoot::setRegion(int region)
{
    vanilla_set_region(region);
}

void BackendViaLocalRoot::setBatteryStatus(int status)
{
    vanilla_set_battery_status(status);
}

void BackendViaLocalRoot::syncFutureCompleted()
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

void BackendPipe::sync(uint16_t code)
{
    m_process->start(QStringLiteral("pkexec"), {pipeProcessFilename(), m_wirelessInterface, QStringLiteral("-sync"), QString::number(code)});
}

void BackendPipe::connectToConsole()
{
    m_process->start(QStringLiteral("pkexec"), {pipeProcessFilename(), m_wirelessInterface, QStringLiteral("-connect")});
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
            // Do nothing?
        } else {
            printf("%s\n", a.constData());
        }
    }
}

void BackendPipe::quit()
{
    if (m_process) {
        // FIXME: Currently, terminate() appears to fail because of pkexec permission issues. Will have to find a workaround...
        printf("terminate\n");
        m_process->terminate();
        printf("wait for finished\n");
        m_process->waitForFinished();
        printf("delete later\n");
        m_process->deleteLater();
        m_process = nullptr;
    }
}