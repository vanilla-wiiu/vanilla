#include "backend.h"

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
    {
        // Convert audio byte length into time
        //qint64 f = dataLength * 1000 / (48000 * (16 / 8) * 2);
        //printf("rumbling for %lu bytes or %li ms\n", dataLength, f);
        // TODO: Not sure what the correct amount of time to vibrate for is, will need to do more research
        emit backend->vibrate(1000);
        break;
    }
    }
}

Backend::Backend(QObject *parent) : QObject(parent)
{

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
