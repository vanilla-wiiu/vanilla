#include "syncprogressdialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QMessageBox>
#include <QVBoxLayout>
#include <vanilla.h>

#include "mainwindow.h"

SyncProgressDialog::SyncProgressDialog(Backend *backend, uint16_t code, QWidget *parent) : QDialog(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    m_headerLabel = new QLabel(tr("<html><b>Connecting to the Wii U console...</b></html>"), this);
    m_headerLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_headerLabel);

    QHBoxLayout *animationLayout = new QHBoxLayout();
    layout->addLayout(animationLayout);

    animationLayout->addStretch();
    for (size_t i = 0; i < sizeof(m_animationLabels)/sizeof(QLabel *); i++) {
        QLabel *l = new QLabel(this);
        l->setFixedWidth(l->sizeHint().height());
        l->setAlignment(Qt::AlignCenter);
        animationLayout->addWidget(l);
        m_animationLabels[i] = l;
    }
    animationLayout->addStretch();

    m_animationTimer = new QTimer(this);
    m_animationTimer->setInterval(50);
    m_progressAnimation = 0;
    m_animationTimer->start();
    connect(m_animationTimer, &QTimer::timeout, this, &SyncProgressDialog::animationStep);
    animationStep();

    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setText(tr("(This may take some time and multiple attempts depending on your hardware...)"));
    layout->addWidget(m_statusLabel);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    buttons->setCenterButtons(true);
    
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    layout->addWidget(buttons); 

    setWindowTitle(tr("Syncing..."));

    m_cancelled = false;

    m_backend = backend;
    connect(m_backend, &Backend::syncCompleted, this, &SyncProgressDialog::syncReturned);
    QMetaObject::invokeMethod(m_backend, "sync", Q_ARG(uint16_t, code));
}

void SyncProgressDialog::syncReturned(bool success)
{
    if (success) {
        QMessageBox::information(this, QString(), tr("Successfully synced with console"));
    } else {
        if (!m_cancelled) {
            QMessageBox::critical(this, QString(), tr("Something went wrong trying to sync"));
        }
    }
    this->accept();
}

void SyncProgressDialog::done(int r)
{
    return QDialog::done(r);
}

void SyncProgressDialog::reject()
{
    m_headerLabel->setText(tr("<html><b>Cancelling...</b></html>"));
    m_cancelled = true;
    m_backend->interrupt();
}

void SyncProgressDialog::animationStep()
{
    QString s;

    const int beats = sizeof(m_animationLabels)/sizeof(QLabel *);

    //const int big = (m_progressAnimation % beats);
    const int big = (qSin(m_progressAnimation * 0.2) * 0.5 + 0.5) * beats;

    for (int i = 0; i < beats; i++) {
        QLabel *l = m_animationLabels[i];
        if (i == big) {
            l->setText(QString::fromUtf8("\xE2\xAC\xA4"));
        } else {
            l->setText(QString::fromUtf8("\xE2\x80\xA2"));
        }
    }

    m_progressAnimation++;
}