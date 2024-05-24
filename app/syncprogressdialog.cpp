#include "syncprogressdialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QMessageBox>
#include <QVBoxLayout>
#include <vanilla.h>

#include "mainwindow.h"

SyncProgressDialog::SyncProgressDialog(Backend *backend, const QString &wirelessInterface, uint16_t code, QWidget *parent) : QDialog(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    m_headerLabel = new QLabel(tr("<html><b>Connecting to the Wii U console...</b></html>"));
    m_headerLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_headerLabel);

    m_statusLabel = new QLabel(this);
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
    QMetaObject::invokeMethod(m_backend, &Backend::sync, wirelessInterface, code);
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