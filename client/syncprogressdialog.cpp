#include "syncprogressdialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QMessageBox>
#include <QVBoxLayout>
#include <vanilla.h>

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
    layout->addWidget(m_statusLabel);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    buttons->setCenterButtons(true);
    
    //
    // TODO: When cancelling, send SIGINT to server so it will re-enable NetworkManager on the device
    //
    
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    layout->addWidget(buttons); 

    setWindowTitle(tr("Syncing..."));

    m_watcher = new QFutureWatcher<int>(this);
    connect(m_watcher, &QFutureWatcher<int>::finished, this, &SyncProgressDialog::serverReturned);
    m_watcher->setFuture(QtConcurrent::run(startVanillaServer, wirelessInterface, code));
}

void SyncProgressDialog::readFromProcess(QProcess *p)
{
    while (p->canReadLine()) {
        QByteArray line = p->readLine();

        m_statusLabel->setText(line);

        if (line == QByteArrayLiteral("READY\n")) {
            QMessageBox::information(this, QString(), tr("server is ready :) sending sync command"));
            p->write(QStringLiteral("SYNC %1 %2\n").arg(m_wirelessInterface, QString::number(m_wpsCode)).toUtf8());
        } else if (line == QByteArrayLiteral("SUCCESS\n")) {
            QMessageBox::information(this, QString(), tr("Successfully synced with console"));
            this->close();
        }
    }
}

void SyncProgressDialog::serverHasOutput()
{
    QProcess *p = static_cast<QProcess *>(sender());
    
    readFromProcess(p);
}

void SyncProgressDialog::serverReturned()
{
    if (m_watcher->result() == VANILLA_SUCCESS) {
        QMessageBox::information(this, QString(), tr("Successfully synced with console"));
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