#include "backend.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QMessageBox>
#include <QNetworkDatagram>
#include <QtConcurrent/QtConcurrent>

#include <arpa/inet.h>
#include <fcntl.h>
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

BackendViaLocalRoot::BackendViaLocalRoot(const QString &wirelessInterface, QObject *parent) : Backend(parent)
{
    m_wirelessInterface = wirelessInterface;
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
    QtConcurrent::run(connectInternal, this, m_wirelessInterface);
}

int BackendViaLocalRoot::connectInternal(BackendViaLocalRoot *instance, const QString &intf)
{
    QByteArray wirelessInterfaceC = intf.toUtf8();
    return vanilla_start(vanillaEventHandler, instance);
    //return vanilla_connect_to_console(wirelessInterfaceC.constData(), vanillaEventHandler, instance);
    // printf("TEMPORARILY STUBBED\n");
    // return 0;
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

void BackendViaLocalRoot::sync(uint16_t code)
{
    QFutureWatcher<int> *watcher = new QFutureWatcher<int>(this);
    connect(watcher, &QFutureWatcher<int>::finished, this, &BackendViaLocalRoot::syncFutureCompleted);
    watcher->setFuture(QtConcurrent::run(syncInternal, m_wirelessInterface, code));
}

int BackendViaLocalRoot::syncInternal(const QString &intf, uint16_t code)
{
    QByteArray wirelessInterfaceC = intf.toUtf8();
    //return vanilla_sync_with_console(wirelessInterfaceC.constData(), code);
    printf("TEMPORARILY STUBBED\n");
    return 0;
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
}

BackendPipe::~BackendPipe()
{
    waitForFinished();

    QFile::remove(m_pipeInFilename);
    QFile::remove(m_pipeOutFilename);
}

void BackendPipe::waitForFinished()
{
    if (m_process) {
        m_process->waitForFinished();
    }
}

void BackendPipe::start()
{
    const QString pipe_bin = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("vanilla-pipe"));
    m_process = new QProcess(this);
    m_process->setReadChannel(QProcess::StandardError);
    connect(m_process, &QProcess::readyReadStandardError, this, &BackendPipe::receivedData);
    connect(m_process, &QProcess::finished, this, &BackendPipe::closed);

    m_pipeOutFilename = QStringLiteral("/tmp/vanilla-fifo-in-%0").arg(QString::number(QDateTime::currentMSecsSinceEpoch()));
    m_pipeInFilename = QStringLiteral("/tmp/vanilla-fifo-out-%0").arg(QString::number(QDateTime::currentMSecsSinceEpoch()));

    m_process->start(QStringLiteral("pkexec"), {pipe_bin, m_wirelessInterface, QStringLiteral("-pipe"), m_pipeOutFilename, m_pipeInFilename});
}

void BackendPipe::receivedData()
{
    while (m_process->canReadLine()) {
        QByteArray a = m_process->readLine().trimmed();
        if (a == QByteArrayLiteral("READY")) {
            emit pipesAvailable(m_pipeInFilename, m_pipeOutFilename);
        } else {
            printf("%s\n", a.constData());
        }
    }
}