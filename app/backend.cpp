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

#include "../pipe/pipe.h"

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

BackendViaPipe::BackendViaPipe(QObject *parent) : Backend(parent)
{
}

BackendViaSocket::BackendViaSocket(const QHostAddress &backendAddr, quint16 backendPort, QObject *parent) : BackendViaPipe(parent)
{
    m_backendAddress = backendAddr;
    m_backendPort = backendPort;

    m_socket = new QUdpSocket(this);
    connect(m_socket, &QUdpSocket::readyRead, this, &BackendViaSocket::readPendingDatagrams);
    connect(m_socket, &QUdpSocket::disconnected, this, &BackendViaSocket::closed);
}

void BackendViaSocket::init()
{
    bool connected = false;

    for (m_frontendPort = 10100; m_frontendPort < 10200; m_frontendPort++) {
        printf("Trying to bind to port %u\n", m_frontendPort);
        if (m_socket->bind(QHostAddress::Any, m_frontendPort)) {
            printf("Bound to port %u\n", m_frontendPort);
            connected = true;
            break;
        }
    }

    if (connected) {
        // Send bind command to backend
        pipe_control_code cmd;
        cmd.code = VANILLA_PIPE_IN_BIND;
        writeToPipe(&cmd, sizeof(cmd));
    } else {
        printf("Failed to bind to port\n");
        emit error(tr("Failed to bind to UDP port"));
        emit closed();
    }
}

void BackendViaPipe::quitPipe()
{
    pipe_control_code cmd;
    cmd.code = VANILLA_PIPE_IN_QUIT;
    writeToPipe(&cmd, sizeof(cmd));
}

void BackendViaLocalRoot::interrupt()
{
    vanilla_stop();
}

void BackendViaPipe::interrupt()
{
    pipe_control_code cmd;
    cmd.code = VANILLA_PIPE_IN_INTERRUPT;
    writeToPipe(&cmd, sizeof(cmd));
}

void BackendViaLocalRoot::requestIDR()
{
    vanilla_request_idr();
}

void BackendViaPipe::requestIDR()
{
    pipe_control_code cmd;
    cmd.code = VANILLA_PIPE_IN_REQ_IDR;
    writeToPipe(&cmd, sizeof(cmd));
}

void BackendViaLocalRoot::connectToConsole()
{
    QtConcurrent::run(connectInternal, this, m_wirelessInterface);
}

int BackendViaLocalRoot::connectInternal(BackendViaLocalRoot *instance, const QString &intf)
{
    QByteArray wirelessInterfaceC = intf.toUtf8();
    return vanilla_connect_to_console(wirelessInterfaceC.constData(), vanillaEventHandler, instance);
}

void BackendViaPipe::processPacket(const QByteArray &arr)
{
    const pipe_control_code *cc = (const pipe_control_code *) arr.data();

    switch (cc->code) {
    case VANILLA_PIPE_OUT_EOF:
        // Do nothing?
        break;
    case VANILLA_PIPE_OUT_DATA:
    {
        pipe_data_command *event = (pipe_data_command *) cc;
        event->data_size = ntohs(event->data_size);
        vanillaEventHandler(this, event->event_type, (const char *) event->data, event->data_size);
        break;
    }
    case VANILLA_PIPE_OUT_BOUND_SUCCESSFUL:
        emit ready();
        break;
    case VANILLA_PIPE_ERR_SUCCESS:
        printf("ERR_SUCCESS code\n");
        break;
    case VANILLA_PIPE_OUT_SYNC_STATE:
    {
        const pipe_sync_state_command *ss = (const pipe_sync_state_command *) cc;
        emit syncCompleted(ss->state == VANILLA_SUCCESS);
        break;
    }
    }
}

void BackendViaPipe::connectToConsole()
{
    // Request pipe to connect
    pipe_control_code conn_cmd;
    conn_cmd.code = VANILLA_PIPE_IN_CONNECT;
    writeToPipe(&conn_cmd, sizeof(conn_cmd));
}

void BackendViaLocalRoot::updateTouch(int x, int y)
{
    vanilla_set_touch(x, y);
}

void BackendViaPipe::updateTouch(int x, int y)
{
    pipe_touch_command cmd;
    cmd.base.code = VANILLA_PIPE_IN_TOUCH;
    cmd.x = htonl(x);
    cmd.y = htonl(y);
    writeToPipe(&cmd, sizeof(cmd));
}

void BackendViaLocalRoot::setButton(int button, int32_t value)
{
    vanilla_set_button(button, value);
}

void BackendViaPipe::setButton(int button, int32_t value)
{
    pipe_button_command cmd;
    cmd.base.code = VANILLA_PIPE_IN_BUTTON;
    cmd.id = htonl(button);
    cmd.value = htonl(value);
    writeToPipe(&cmd, sizeof(cmd));
}

void BackendViaLocalRoot::setRegion(int region)
{
    vanilla_set_region(region);
}

void BackendViaPipe::setRegion(int region)
{
    pipe_region_command cmd;
    cmd.base.code = VANILLA_PIPE_IN_REGION;
    cmd.region = region;
    writeToPipe(&cmd, sizeof(cmd));
}

void BackendViaLocalRoot::setBatteryStatus(int status)
{
    vanilla_set_battery_status(status);
}

void BackendViaPipe::setBatteryStatus(int status)
{
    pipe_battery_command cmd;
    cmd.base.code = VANILLA_PIPE_IN_BATTERY;
    cmd.battery = status;
    writeToPipe(&cmd, sizeof(cmd));
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
    return vanilla_sync_with_console(wirelessInterfaceC.constData(), code);
}

void BackendViaLocalRoot::syncFutureCompleted()
{
    QFutureWatcher<int> *watcher = static_cast<QFutureWatcher<int>*>(sender());
    int r = watcher->result();
    emit syncCompleted(r == VANILLA_SUCCESS);
    watcher->deleteLater();
}

void BackendViaPipe::sync(uint16_t code)
{
    // Request pipe to sync
    pipe_sync_command cmd;
    cmd.base.code = VANILLA_PIPE_IN_SYNC;
    cmd.code = htons(code);
    writeToPipe(&cmd, sizeof(cmd));
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

void BackendViaSocket::readPendingDatagrams()
{
    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        processPacket(datagram.data());
    }
}

ssize_t BackendViaSocket::writeToPipe(const void *data, size_t length)
{
    return m_socket->writeDatagram((const char *) data, length, m_backendAddress, m_backendPort);
}