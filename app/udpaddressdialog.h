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
    inline const quint16 &acceptedPort() const { return m_acceptedPort; }

public slots:
    virtual void done(int r) override;

private:
    QLineEdit *m_addressLine;
    QHostAddress m_acceptedAddress;
    quint16 m_acceptedPort;
};

#endif // UDP_ADDRESS_DIALOG