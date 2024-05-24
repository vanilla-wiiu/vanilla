#include "backend.h"

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
        emit backend->vibrate(data != nullptr);
        break;
    }
}

Backend::Backend(QObject *parent) : QObject(parent)
{
    // If not running as root, use pipe
    m_usePipe = (geteuid() != 0);
    m_pipe = nullptr;
    m_pipeIn = -1;
    m_pipeOut = -1;
}

void Backend::interrupt()
{
    vanilla_stop();
}

void Backend::connectToConsole(const QString &wirelessInterface)
{
    QByteArray wirelessInterfaceC = wirelessInterface.toUtf8();
    vanilla_connect_to_console(wirelessInterfaceC.constData(), vanillaEventHandler, this);
}

void Backend::updateTouch(int x, int y)
{
    vanilla_set_touch(x, y);
}

void Backend::setButton(int button, int16_t value)
{
    vanilla_set_button(button, value);
}

void Backend::sync(const QString &wirelessInterface, uint16_t code)
{
    if (m_usePipe) {
        // Get pipe handles
        int in = getInPipe();
        int out = getOutPipe();

        // Request pipe to sync
        m_pipeMutex.lock();
        uint8_t cc = VANILLA_PIPE_IN_SYNC;
        write(out, &cc, sizeof(cc));
        m_pipeMutex.unlock();

        // See if pipe accepted our request to sync
        read(in, &cc, sizeof(cc));
        if (cc != VANILLA_PIPE_ERR_SUCCESS) {
            emit syncCompleted(false);
            return;
        }

        // Wait for sync status
        read(in, &cc, sizeof(cc));
        if (cc == VANILLA_PIPE_OUT_SYNC_STATE) {
            read(in, &cc, sizeof(cc));
            emit syncCompleted(cc);
        } else {
            emit syncCompleted(false);
        }
    } else {
        QByteArray wirelessInterfaceC = wirelessInterface.toUtf8();
        int r = vanilla_sync_with_console(wirelessInterfaceC.constData(), code);
        emit syncCompleted(r == VANILLA_SUCCESS);
    }
}

int Backend::getInPipe()
{
    ensurePipes();
    if (m_pipeIn == 0) {
        // The pipe's output is our input
        m_pipeIn = open(VANILLA_PIPE_OUT_FILENAME, O_RDONLY);
    }

    return m_pipeIn;
}

int Backend::getOutPipe()
{
    ensurePipes();
    if (m_pipeOut == 0) {
        // The pipe's input is our output
        m_pipeOut = open(VANILLA_PIPE_IN_FILENAME, O_WRONLY);
    }

    return m_pipeOut;
}

bool Backend::ensurePipes()
{
    if (!m_pipe) {
        m_pipe = new QProcess(this);
        m_pipe->start(QStringLiteral("pkexec"), {QStringLiteral("vanilla-pipe")});
    }
    return true;
}