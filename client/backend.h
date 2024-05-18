#ifndef BACKEND_H
#define BACKEND_H

#include <QObject>

class Backend : public QObject
{
    Q_OBJECT
public:
    Backend(QObject *parent = nullptr);

    void interrupt();

signals:
    void videoAvailable(const QByteArray &packet);
    void audioAvailable(const QByteArray &packet);
    void vibrate(qint64 duration);
    void errorOccurred();

public slots:
    void connectToConsole(const QString &wirelessInterface);

};

#endif // BACKEND_H