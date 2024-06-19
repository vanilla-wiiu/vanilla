#ifndef BACKEND_H
#define BACKEND_H

#include <QMutex>
#include <QObject>
#include <QProcess>
#include <QThread>

class BackendPipe : public QObject
{
    Q_OBJECT
public:
    BackendPipe(QObject *parent = nullptr);
    
    virtual ~BackendPipe() override;

    void waitForFinished();

public slots:
    void start();

signals:
    void pipesAvailable(const QByteArray &in, const QByteArray &out);

private slots:
    void receivedData();

private:
    QProcess *m_process;
    QByteArray m_pipeOutFilename;
    QByteArray m_pipeInFilename;

};

class Backend : public QObject
{
    Q_OBJECT
public:
    Backend(QObject *parent = nullptr);

    virtual ~Backend() override;

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
    void setButton(int button, int32_t value);
    void requestIDR();
    void setRegion(int region);

private:
    BackendPipe *m_pipe;
    QThread *m_pipeThread;
    int m_pipeIn;
    int m_pipeOut;
    QMutex m_pipeMutex;
    QAtomicInt m_interrupt;

private slots:
    void setUpPipes(const QByteArray &in, const QByteArray &out);

};

#endif // BACKEND_H