#ifndef BACKEND_H
#define BACKEND_H

#include <QMutex>
#include <QObject>
#include <QProcess>

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

private:
    int getInPipe();
    int getOutPipe();
    bool ensurePipes();

    bool m_usePipe;
    QProcess *m_pipe;
    int m_pipeIn;
    int m_pipeOut;
    QMutex m_pipeMutex;

};

#endif // BACKEND_H