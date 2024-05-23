#include "syncprogressdialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QMessageBox>
#include <QVBoxLayout>
#include <vanilla.h>

#include "mainwindow.h"

int startVanillaServer(const QString &wirelessInterface, uint16_t code)
{
    QByteArray wirelessInterfaceC = wirelessInterface.toUtf8();
    return vanilla_sync_with_console(wirelessInterfaceC.constData(), code);
}

SyncProgressDialog::SyncProgressDialog(const QString &wirelessInterface, uint16_t code, QWidget *parent) : QDialog(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(tr("Connecting to the Wii U console...")));

    m_statusLabel = new QLabel(this);
    m_statusLabel->setText(tr("(This may take some time and multiple attempts depending on your hardware...)"));
    layout->addWidget(m_statusLabel);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    buttons->setCenterButtons(true);
    
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    layout->addWidget(buttons); 

    setWindowTitle(tr("Syncing..."));

    m_watcher = new QFutureWatcher<int>(this);
    connect(m_watcher, &QFutureWatcher<int>::finished, this, &SyncProgressDialog::serverReturned);
    m_watcher->setFuture(QtConcurrent::run(startVanillaServer, wirelessInterface, code));
}

void SyncProgressDialog::serverReturned()
{
    if (m_watcher->result() == VANILLA_SUCCESS) {
        QMessageBox::information(this, QString(), tr("Successfully synced with console"));
        MainWindow::instance()->enableConnectButton();
    } else {
        QMessageBox::critical(this, QString(), tr("Something went wrong trying to sync"));
    }
    m_watcher->deleteLater();
    m_watcher = nullptr;
    this->close();
}

void SyncProgressDialog::done(int r)
{
    if (m_watcher) {
        disconnect(m_watcher, &QFutureWatcher<int>::finished, this, &SyncProgressDialog::serverReturned);
        vanilla_stop();
        m_watcher->waitForFinished();
        m_watcher->deleteLater();
        m_watcher = nullptr;
    }
    
    return QDialog::done(r);
}