#ifndef BACKEND_H
#define BACKEND_H

#include <QBuffer>
#include <QMutex>
#include <QObject>
#include <QProcess>
#include <QThread>
#include <QUdpSocket>
#include <QWaitCondition>

class BackendPipe : public QObject
{
    Q_OBJECT
public:
    BackendPipe(const QString &wirelessInterface, QObject *parent = nullptr);
    
    virtual ~BackendPipe() override;

    void waitForFinished();

public slots:
    void start();

signals:
    void pipesAvailable(const QString &in, const QString &out);
    void portAvailable(uint16_t port);
    void closed();

private slots:
    void receivedData();

private:
    QProcess *m_process;
    QString m_pipeOutFilename;
    QString m_pipeInFilename;
    uint16_t m_serverPort;
    uint16_t m_clientPort;
    QString m_wirelessInterface;

};

class Backend : public QObject
{
    Q_OBJECT
public:
    Backend(QObject *parent = nullptr);

    // These are all commands that can be issued to the backend. They are re-entrant and can be called at any time.
    virtual void interrupt() = 0;
    virtual void updateTouch(int x, int y) = 0;
    virtual void setButton(int button, int32_t value) = 0;
    virtual void requestIDR() = 0;
    virtual void setRegion(int region) = 0;
    virtual void setBatteryStatus(int status) = 0;

signals:
    void videoAvailable(const QByteArray &packet);
    void audioAvailable(const QByteArray &packet);
    void vibrate(bool on);
    void errorOccurred();
    void syncCompleted(bool success);
    void ready();
    void closed();
    void error(const QString &err);

public slots:
    // These slots must be called with Qt::QueuedConnection to start the event loops in the backend's thread
    virtual void init();
    virtual void sync(uint16_t code) = 0;
    virtual void connectToConsole() = 0;

};

class BackendViaLocalRoot : public Backend
{
    Q_OBJECT
public:
    BackendViaLocalRoot(const QString &wirelessInterface, QObject *parent = nullptr);

    virtual void interrupt() override;
    virtual void updateTouch(int x, int y) override;
    virtual void setButton(int button, int32_t value) override;
    virtual void requestIDR() override;
    virtual void setRegion(int region) override;
    virtual void setBatteryStatus(int status) override;

public slots:
    virtual void sync(uint16_t code) override;
    virtual void connectToConsole() override;

private:
    static int syncInternal(const QString &intf, uint16_t code);
    static int connectInternal(BackendViaLocalRoot *instance, const QString &intf);
    QString m_wirelessInterface;

private slots:
    void syncFutureCompleted();

};

#endif // BACKEND_H