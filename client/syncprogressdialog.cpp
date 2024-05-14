#include "syncprogressdialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrent>
#include <QVBoxLayout>

extern "C" {
#include "server.h"
}

SyncProgressDialog::SyncProgressDialog(QWidget *parent) : QDialog(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(tr("Connecting to the Wii U console...")));

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    buttons->setCenterButtons(true);
    
    //
    // TODO: When cancelling, send SIGINT to server so it will re-enable NetworkManager on the device
    //
    
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    layout->addWidget(buttons);

    setWindowTitle(tr("Syncing..."));
}

int startVanillaServer(const QString &wirelessInterface, uint16_t code)
{
    return vanilla_sync_with_console(wirelessInterface.toUtf8(), code);
}

void SyncProgressDialog::start(const QString &wirelessInterface, uint16_t code)
{
    QFutureWatcher<int> *watcher = new QFutureWatcher<int>(this);
    connect(watcher, &QFutureWatcher<int>::finished, this, &SyncProgressDialog::serverReturned);
    watcher->setFuture(QtConcurrent::run(startVanillaServer, wirelessInterface, code));
}

void SyncProgressDialog::serverReturned()
{
    QFutureWatcher<int> *watcher = static_cast<QFutureWatcher<int> *>(sender());
    QMessageBox::information(this, QString(), tr("Server returned code: %1").arg(watcher->result()));
    watcher->deleteLater();
    this->close();
}