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
    SyncProgressDialog(Backend *backend, uint16_t code, QWidget *parent = nullptr);

protected:
    virtual void done(int r) override;
    virtual void reject() override;

private:
    Backend *m_backend;
    QLabel *m_headerLabel;
    QLabel *m_statusLabel;
    uint16_t m_wpsCode;
    bool m_cancelled;

    QLabel *m_animationLabels[8];
    int m_progressAnimation;
    QTimer *m_animationTimer;

private slots:
    void syncReturned(bool success);

    void animationStep();

};

#endif // SYNC_PROGRESS_H