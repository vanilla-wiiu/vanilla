#ifndef BACKEND_H
#define BACKEND_H

#include <QObject>

class Backend : public QObject
{
    Q_OBJECT
public:
    Backend(QObject *parent = nullptr);

    void handleEvent(int type, const char *data, size_t dataLength);

    void interrupt();

signals:
    void videoAvailable(const QByteArray &packet);
    void audioAvailable(const QByteArray &packet);
    void errorOccurred();

public slots:
    void connectToConsole(const QString &wirelessInterface);

};

#endif // BACKEND_H