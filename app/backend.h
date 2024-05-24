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
    void vibrate(bool on);
    void errorOccurred();
    void syncCompleted(bool success);

public slots:
    void sync(const QString &wirelessInterface, uint16_t code);
    void connectToConsole(const QString &wirelessInterface);
    void updateTouch(int x, int y);
    void setButton(int button, int16_t value);

};

#endif // BACKEND_H