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

public slots:
    void start();
    void quit();

signals:
    void pipeAvailable();
    void closed();

private slots:
    void receivedData();

private:
    static QString pipeProcessFilename();

    QProcess *m_process;
    QString m_socketFilename;
    QString m_wirelessInterface;

};

class Backend : public QObject
{
    Q_OBJECT
public:
    Backend(QObject *parent = nullptr);

    // These are all commands that can be issued to the backend. They are re-entrant and can be called at any time.
    void interrupt();
    void updateTouch(int x, int y);
    void setButton(int button, int32_t value);
    void requestIDR();
    void setRegion(int region);
    void setBatteryStatus(int status);

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
    void init();
    void sync(uint16_t code);
    void connectToConsole();

protected:
    virtual int initInternal();
    virtual int syncInternal(uint16_t code);
    virtual int connectInternal();

private slots:
    void syncFutureCompleted();

    void vanillaEventHandler();

};

class BackendViaInternalPipe : public Backend
{
    Q_OBJECT
public:
    BackendViaInternalPipe(const QString &wirelessInterface, QObject *parent = nullptr);

protected:
    virtual int initInternal() override;
    virtual int syncInternal(uint16_t code) override;
    virtual int connectInternal() override;

private:
    QString m_wirelessInterface;
    BackendPipe *m_pipe;
};

class BackendViaExternalPipe : public Backend
{
    Q_OBJECT
public:
    BackendViaExternalPipe(const QHostAddress &serverAddress, QObject *parent = nullptr);

protected:
    // virtual int initInternal() override;
    virtual int syncInternal(uint16_t code) override;
    virtual int connectInternal() override;

private:
    QHostAddress m_serverAddress;
};

#endif // BACKEND_H