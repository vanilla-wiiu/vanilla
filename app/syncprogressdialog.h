#ifndef SYNC_PROGRESS_H
#define SYNC_PROGRESS_H

#include <QDialog>
#include <QLabel>
#include <QProcess>
#include <QtConcurrent/QtConcurrent>

#include "backend.h"

class SyncProgressDialog : public QDialog
{
    Q_OBJECT
public:
    SyncProgressDialog(Backend *backend, const QString &wirelessInterface, uint16_t code, QWidget *parent = nullptr);

protected:
    virtual void done(int r) override;
    virtual void reject() override;

private:
    Backend *m_backend;
    QLabel *m_headerLabel;
    QLabel *m_statusLabel;
    QString m_wirelessInterface;
    uint16_t m_wpsCode;
    bool m_cancelled;

private slots:
    void syncReturned(bool success);

};

#endif // SYNC_PROGRESS_H