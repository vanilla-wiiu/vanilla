#include "udpaddressdialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

UdpAddressDialog::UdpAddressDialog(QWidget *parent) : QDialog(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(tr("<html><center><p><b>UDP Server Connection</b></p><p>Connect to a separate device handling the connection to the console.</p></center></html>")));

    m_addressLine = new QLineEdit(this);
    m_addressLine->setPlaceholderText(QStringLiteral("127.0.0.1"));
    layout->addWidget(m_addressLine);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setCenterButtons(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void UdpAddressDialog::done(int r)
{
    if (r == QDialog::Accepted) {
        if (m_addressLine->text().isEmpty()) {
            QMessageBox::critical(this, tr("Invalid Address"), tr("No address entered."));
            return;
        }

        QHostAddress addressPortion = QHostAddress(m_addressLine->text());
        if (addressPortion.isNull()) {
            QMessageBox::critical(this, tr("Invalid Address"), tr("Address is invalid."));
            return;
        }

        m_acceptedAddress = addressPortion;
    }

    QDialog::done(r);
}