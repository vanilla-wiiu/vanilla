#ifndef SYNC_PROGRESS_H
#define SYNC_PROGRESS_H

#include <QDialog>
#include <QLabel>
#include <QProcess>
#include <QtConcurrent/QtConcurrent>

class SyncProgressDialog : public QDialog
{
    Q_OBJECT
public:
    SyncProgressDialog(const QString &wirelessInterface, uint16_t code, QWidget *parent = nullptr);

protected:
    virtual void done(int r) override;

private:
    void readFromProcess(QProcess *p);

    QLabel *m_statusLabel;
    QString m_wirelessInterface;
    uint16_t m_wpsCode;

    QFutureWatcher<int> *m_watcher;

private slots:
    void serverHasOutput();
    void serverReturned();

};

#endif // SYNC_PROGRESS_H