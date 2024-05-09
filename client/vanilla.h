#ifndef VANILLA_H
#define VANILLA_H

#include <QComboBox>
#include <QProcess>
#include <QWidget>

class Vanilla : public QWidget
{
public:
    Vanilla(QWidget *parent = nullptr);

private:
    void populateWirelessInterfaces();
    void populateMicrophones();
    void populateControllers();

    QComboBox *m_wirelessInterfaceComboBox;
    QComboBox *m_microphoneComboBox;
    QComboBox *m_controllerComboBox;
    QProcess *m_process;

private slots:
    void showSyncDialog();

};

#endif // VANILLA_H
