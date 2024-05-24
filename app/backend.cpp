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
        emit backend->vibrate(data != nullptr);
        break;
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
    QByteArray wirelessInterfaceC = wirelessInterface.toUtf8();
    int r = vanilla_sync_with_console(wirelessInterfaceC.constData(), code);
    emit syncCompleted(r == VANILLA_SUCCESS);
}