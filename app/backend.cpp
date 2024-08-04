#include "backend.h"

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

BackendViaNamedPipe::BackendViaNamedPipe(const QString &wirelessInterface, QObject *parent) : BackendViaPipe(parent)
{
    m_pipeIn = -1;
    m_pipeOut = -1;
    m_pipeThread = new QThread(this);
    m_pipe = new BackendPipe(wirelessInterface, this);
    connect(m_pipe, &BackendPipe::pipesAvailable, this, &BackendViaNamedPipe::setUpPipes);
    connect(m_pipe, &BackendPipe::closed, this, &BackendViaNamedPipe::closed);
}

void BackendViaNamedPipe::init()
{
    m_pipe->setParent(nullptr);
    m_pipeThread->start();
    m_pipe->moveToThread(m_pipeThread);
    QMetaObject::invokeMethod(m_pipe, &BackendPipe::start, Qt::QueuedConnection);
}

BackendViaSocket::BackendViaSocket(const QHostAddress &backendAddr, quint16 backendPort, QObject *parent) : BackendViaPipe(parent)
{
    m_socket = new BackendUdpWrapper(backendAddr, backendPort, this);
    connect(m_socket, &BackendUdpWrapper::socketReady, this, &BackendViaSocket::socketReady);
    connect(m_socket, &BackendUdpWrapper::receivedData, this, &BackendViaSocket::receivedData, Qt::DirectConnection);
    connect(m_socket, &BackendUdpWrapper::closed, this, &BackendViaSocket::closed);
    connect(m_socket, &BackendUdpWrapper::error, this, &BackendViaSocket::error);

    m_socketThread = new QThread(this);
}

BackendViaSocket::~BackendViaSocket()
{
    m_socket->deleteLater();
    m_socketThread->quit();
    m_socketThread->wait();
}

void BackendViaSocket::init()
{
    m_socketThread->start();
    m_socket->setParent(nullptr);
    m_socket->moveToThread(m_socketThread);
    QMetaObject::invokeMethod(m_socket, &BackendUdpWrapper::start, Qt::QueuedConnection);
}

void BackendViaPipe::quitPipe()
{
    pipe_control_code cmd;
    cmd.code = VANILLA_PIPE_IN_QUIT;
    writeToPipe(&cmd, sizeof(cmd));
}

BackendViaNamedPipe::~BackendViaNamedPipe()
{
    quitPipe();

    m_pipe->deleteLater();
    m_pipeThread->quit();
    m_pipeThread->wait();
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

void BackendViaPipe::connectToConsole()
{
    // Request pipe to connect
    pipe_control_code conn_cmd;
    conn_cmd.code = VANILLA_PIPE_IN_CONNECT;
    writeToPipe(&conn_cmd, sizeof(conn_cmd));

    uint8_t cmd[UINT16_MAX];
    while (true) {
        ssize_t read_size = readFromPipe(cmd, sizeof(cmd));
        if (read_size == 0) {
            continue;
        }

        pipe_control_code *cc = (pipe_control_code *) cmd;

        if (cc->code == VANILLA_PIPE_OUT_EOF) {
            break;
        } else if (cc->code == VANILLA_PIPE_OUT_DATA) {
            pipe_data_command *event = (pipe_data_command *) cmd;
            event->data_size = ntohs(event->data_size);
            vanillaEventHandler(this, event->event_type, (const char *) event->data, event->data_size);
        }
    }
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

    // See if pipe accepted our request to sync
    uint8_t cc;
    readFromPipe(&cc, sizeof(cc));
    if (cc != VANILLA_PIPE_ERR_SUCCESS) {
        emit syncCompleted(false);
        return;
    }

    // Wait for sync status
    readFromPipe(&cc, sizeof(cc));
    if (cc == VANILLA_PIPE_OUT_SYNC_STATE) {
        readFromPipe(&cc, sizeof(cc));
        emit syncCompleted(cc == VANILLA_SUCCESS);
    } else {
        emit syncCompleted(false);
    }
}

void BackendViaNamedPipe::setUpPipes(const QString &in, const QString &out)
{
    QByteArray inUtf8 = in.toUtf8();
    QByteArray outUtf8 = out.toUtf8();
    m_pipeIn = open(inUtf8.constData(), O_RDONLY);
    if (m_pipeIn == -1) {
        QMessageBox::critical(nullptr, tr("Pipe Error"), tr("Failed to create in pipe: %1").arg(strerror(errno)));
    }
    m_pipeOut = open(outUtf8.constData(), O_WRONLY);
    if (m_pipeOut == -1) {
        QMessageBox::critical(nullptr, tr("Pipe Error"), tr("Failed to create out pipe: %1").arg(strerror(errno)));
    }

    printf("Established connection with backend\n");
    emit ready();
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

ssize_t BackendViaNamedPipe::readFromPipe(void *data, size_t length)
{
    return read(m_pipeIn, data, length);
}

ssize_t BackendViaNamedPipe::writeToPipe(const void *data, size_t length)
{
    return write(m_pipeOut, data, length);
}

BackendUdpWrapper::BackendUdpWrapper(const QHostAddress &backendAddr, quint16 backendPort, QObject *parent) : QObject(parent)
{
    m_backendAddress = backendAddr;
    m_backendPort = backendPort;

    m_socket = new QUdpSocket(this);
    connect(m_socket, &QUdpSocket::readyRead, this, &BackendUdpWrapper::readPendingDatagrams);
}

void BackendUdpWrapper::start()
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
        emit socketReady(m_frontendPort);
    } else {
        printf("Failed to bind to port\n");
        emit error(tr("Failed to bind to UDP port"));
        emit closed();
    }
}

void BackendUdpWrapper::readPendingDatagrams()
{
    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        emit receivedData(datagram.data());
    }
}

void BackendUdpWrapper::write(const QByteArray &data)
{
    m_socket->writeDatagram(data, m_backendAddress, m_backendPort);
}

ssize_t BackendViaSocket::readFromPipe(void *data, size_t length)
{
    m_readMutex.lock();
    if (m_buffer.isEmpty()) {
        m_readWaitCond.wait(&m_readMutex);
    }
    if (m_buffer.size() < length) {
        length = m_buffer.size();
    }
    memcpy(data, m_buffer.constData(), length);
    m_buffer.remove(0, length);
    m_readMutex.unlock();
    return length;
}

ssize_t BackendViaSocket::writeToPipe(const void *data, size_t length)
{
    QMetaObject::invokeMethod(m_socket, "write", Q_ARG(QByteArray, QByteArray((const char *) data, length)));
    return length;
}

void BackendViaSocket::receivedData(const QByteArray &data)
{
    m_readMutex.lock();
    m_buffer.append(data);
    m_readWaitCond.wakeAll();
    m_readMutex.unlock();
}

void BackendViaSocket::socketReady(quint16 port)
{
    pipe_control_code cmd;
    cmd.code = VANILLA_PIPE_IN_BIND;
    writeToPipe(&cmd, sizeof(cmd));

    uint8_t cc;
    readFromPipe(&cc, sizeof(cc));
    if (cc == VANILLA_PIPE_OUT_BOUND_SUCCESSFUL) {
        emit ready();
    }
}