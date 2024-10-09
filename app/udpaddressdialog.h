#ifndef UDP_ADDRESS_DIALOG
#define UDP_ADDRESS_DIALOG

#include <QDialog>
#include <QHostAddress>
#include <QLineEdit>

class UdpAddressDialog : public QDialog
{
    Q_OBJECT
public:
    UdpAddressDialog(QWidget *parent = nullptr);

    inline const QHostAddress &acceptedAddress() const { return m_acceptedAddress; }

public slots:
    virtual void done(int r) override;

private:
    QLineEdit *m_addressLine;
    QHostAddress m_acceptedAddress;
};

#endif // UDP_ADDRESS_DIALOG