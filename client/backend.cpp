#include "backend.h"

#include <vanilla.h>

void vanillaEventHandler(void *context, int type, const char *data, size_t dataLength)
{
    Backend *backend = static_cast<Backend*>(context);

    backend->handleEvent(type, data, dataLength);
}

Backend::Backend(QObject *parent) : QObject(parent)
{

}

void Backend::interrupt()
{
    vanilla_stop();
}

void Backend::handleEvent(int type, const char *data, size_t dataLength)
{
    switch (type) {
    case VANILLA_EVENT_VIDEO:
        emit videoAvailable(QByteArray(data, dataLength));
        break;
    case VANILLA_EVENT_AUDIO:
        emit audioAvailable(QByteArray(data, dataLength));
        break;
    }
}

void Backend::connectToConsole(const QString &wirelessInterface)
{
    QByteArray wirelessInterfaceC = wirelessInterface.toUtf8();
    vanilla_connect_to_console(wirelessInterfaceC.constData(), vanillaEventHandler, this);
}
