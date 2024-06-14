#include "backend.h"

#include <QCoreApplication>
#include <QDir>
#include <QMessageBox>

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
    m_pipe = nullptr;
    m_pipeIn = -1;
    m_pipeOut = -1;
    m_interrupt = 0;

    // If not running as root, use pipe
    if ((geteuid() != 0)) {
        m_pipeThread = new QThread(this);
        m_pipeThread->start();

        m_pipe = new BackendPipe();
        m_pipe->moveToThread(m_pipeThread);

        connect(m_pipe, &BackendPipe::pipesAvailable, this, &Backend::setUpPipes);

        QMetaObject::invokeMethod(m_pipe, &BackendPipe::start, Qt::QueuedConnection);
    }
}

Backend::~Backend()
{
    if (m_pipe) {
        m_pipeMutex.lock();
        uint8_t cc = VANILLA_PIPE_IN_QUIT;
        write(m_pipeOut, &cc, sizeof(cc));
        m_pipeMutex.unlock();

        m_pipe->deleteLater();
        m_pipeThread->quit();
        m_pipeThread->wait();
    }
}

void ignoreByte(int pipe)
{
    uint8_t ignore;
    read(pipe, &ignore, sizeof(ignore));
}

void writeByte(int pipe, uint8_t byte)
{
    write(pipe, &byte, sizeof(byte));
}

void Backend::interrupt()
{
    if (m_pipe) {
        m_pipeMutex.lock();
        uint8_t cc = VANILLA_PIPE_IN_INTERRUPT;
        write(m_pipeOut, &cc, sizeof(cc));
        m_pipeMutex.unlock();
    } else {
        vanilla_stop();
    }
}

void writeNullTermString(int pipe, const QString &s)
{
    QByteArray sc = s.toUtf8();
    write(pipe, sc.constData(), sc.size());
    writeByte(pipe, 0);
}

void Backend::connectToConsole(const QString &wirelessInterface)
{
    if (m_pipe) {
        // Request pipe to connect
        m_pipeMutex.lock();
        writeByte(m_pipeOut, VANILLA_PIPE_IN_CONNECT);
        writeNullTermString(m_pipeOut, wirelessInterface);
        m_pipeMutex.unlock();

        uint8_t cc;
        while (true) {
            ssize_t read_size = read(m_pipeIn, &cc, sizeof(cc));
            if (read_size == 0) {
                continue;
            }

            if (cc == VANILLA_PIPE_OUT_EOF) {
                break;
            } else if (cc == VANILLA_PIPE_OUT_DATA) {
                // Read event
                read(m_pipeIn, &cc, sizeof(cc));

                // Read data size
                uint64_t data_size;
                read(m_pipeIn, &data_size, sizeof(data_size));

                void *buf = malloc(data_size);
                read(m_pipeIn, buf, data_size);

                vanillaEventHandler(this, cc, (const char *) buf, data_size);

                free(buf);
            }
        }
    } else {
        QByteArray wirelessInterfaceC = wirelessInterface.toUtf8();
        vanilla_connect_to_console(wirelessInterfaceC.constData(), vanillaEventHandler, this);
    }
}

void Backend::updateTouch(int x, int y)
{
    if (m_pipe) {
        m_pipeMutex.lock();
        writeByte(m_pipeOut, VANILLA_PIPE_IN_TOUCH);
        int32_t touchX = x;
        int32_t touchY = y;
        write(m_pipeOut, &x, sizeof(x));
        write(m_pipeOut, &y, sizeof(y));
        m_pipeMutex.unlock();
    } else {
        vanilla_set_touch(x, y);
    }
}

void Backend::setButton(int button, int32_t value)
{
    if (m_pipe) {
        m_pipeMutex.lock();
        writeByte(m_pipeOut, VANILLA_PIPE_IN_BUTTON);
        int32_t buttonSized = button;
        write(m_pipeOut, &buttonSized, sizeof(buttonSized));
        write(m_pipeOut, &value, sizeof(value));
        m_pipeMutex.unlock();
    } else {
        vanilla_set_button(button, value);
    }
}

void Backend::sync(const QString &wirelessInterface, uint16_t code)
{
    if (m_pipe) {
        // Request pipe to sync
        m_pipeMutex.lock();
        
        // Write control code
        writeByte(m_pipeOut, VANILLA_PIPE_IN_SYNC);

        // Write WPS code
        write(m_pipeOut, &code, sizeof(code));

        // Write wireless interface
        writeNullTermString(m_pipeOut, wirelessInterface);

        m_pipeMutex.unlock();

        // See if pipe accepted our request to sync
        uint8_t cc;
        read(m_pipeIn, &cc, sizeof(cc));
        if (cc != VANILLA_PIPE_ERR_SUCCESS) {
            emit syncCompleted(false);
            return;
        }

        // Wait for sync status
        read(m_pipeIn, &cc, sizeof(cc));
        if (cc == VANILLA_PIPE_OUT_SYNC_STATE) {
            read(m_pipeIn, &cc, sizeof(cc));
            emit syncCompleted(cc == VANILLA_SUCCESS);
        } else {
            emit syncCompleted(false);
        }
    } else {
        QByteArray wirelessInterfaceC = wirelessInterface.toUtf8();
        int r = vanilla_sync_with_console(wirelessInterfaceC.constData(), code);
        emit syncCompleted(r == VANILLA_SUCCESS);
    }
}

void Backend::setUpPipes(const QByteArray &in, const QByteArray &out)
{
    m_pipeIn = open(in.constData(), O_RDONLY);
    if (m_pipeIn == -1) {
        QMessageBox::critical(nullptr, tr("Pipe Error"), tr("Failed to create in pipe: %1").arg(strerror(errno)));
    }
    m_pipeOut = open(out.constData(), O_WRONLY);
    if (m_pipeOut == -1) {
        QMessageBox::critical(nullptr, tr("Pipe Error"), tr("Failed to create out pipe: %1").arg(strerror(errno)));
    }

    printf("Established connection with backend\n");
}

BackendPipe::BackendPipe(QObject *parent) : QObject(parent)
{
    m_process = nullptr;
}

BackendPipe::~BackendPipe()
{
    waitForFinished();
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
    //connect(m_pipe, &QProcess::finished, this, [this](int code){printf("closed??? %i\n", code);});
    m_process->start(QStringLiteral("pkexec"), {pipe_bin});
}

void BackendPipe::receivedData()
{
    while (m_process->canReadLine()) {
        QByteArray a = m_process->readLine().trimmed();
        // Their out is our in which is why these are flipped
        if (m_pipeOutFilename.isEmpty()) {
            if (QFile::exists(a)) {
                m_pipeOutFilename = a;
            }
        } else if (m_pipeInFilename.isEmpty()) {
            if (QFile::exists(a)) {
                m_pipeInFilename = a;
                emit pipesAvailable(m_pipeInFilename, m_pipeOutFilename);
            }
        } else {
            printf("%s\n", a.constData());
        }
    }
}