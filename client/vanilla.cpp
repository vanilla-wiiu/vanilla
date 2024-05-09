#include "vanilla.h"

#include <QAudioDevice>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaDevices>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <SDL2/SDL.h>

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/wireless.h>

#include "syncdialog.h"

Vanilla::Vanilla(QWidget *parent) : QWidget(parent)
{
    if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
        QMessageBox::critical(this, tr("SDL2 Error"), tr("SDL2 failed to initialize. Controller support will be unavailable."));
    }

    QHBoxLayout *layout = new QHBoxLayout(this);

    QSplitter *splitter = new QSplitter(this);
    layout->addWidget(splitter);

    QWidget *configSection = new QWidget(this);
    splitter->addWidget(configSection);

    QVBoxLayout *configOuterLayout = new QVBoxLayout(configSection);

    {
        QGroupBox *connectionConfigGroupBox = new QGroupBox(tr("Connection"), configSection);
        configOuterLayout->addWidget(connectionConfigGroupBox);

        QGridLayout *configLayout = new QGridLayout(connectionConfigGroupBox);

        int row = 0;

        configLayout->addWidget(new QLabel(tr("Wi-Fi Adapter: "), connectionConfigGroupBox), row, 0);

        m_wirelessInterfaceComboBox = new QComboBox(connectionConfigGroupBox);
        configLayout->addWidget(m_wirelessInterfaceComboBox, row, 1);

        row++;

        QPushButton *syncBtn = new QPushButton(tr("Sync"), connectionConfigGroupBox);
        connect(syncBtn, &QPushButton::clicked, this, &Vanilla::showSyncDialog);
        configLayout->addWidget(syncBtn, row, 0, 1, 2);

        row++;

        configLayout->addWidget(new QPushButton(tr("Connect"), connectionConfigGroupBox), row, 0, 1, 2);
    }

    {
        QGroupBox *displayConfigGroupBox = new QGroupBox(tr("Display"), configSection);
        configOuterLayout->addWidget(displayConfigGroupBox);

        QGridLayout *configLayout = new QGridLayout(displayConfigGroupBox);

        int row = 0;

        configLayout->addWidget(new QPushButton(tr("Full Screen"), configSection), row, 0, 1, 2);
    }

    {
        QGroupBox *soundConfigGroupBox = new QGroupBox(tr("Sound"), configSection);
        configOuterLayout->addWidget(soundConfigGroupBox);

        QGridLayout *configLayout = new QGridLayout(soundConfigGroupBox);

        int row = 0;

        configLayout->addWidget(new QLabel(tr("Microphone: "), soundConfigGroupBox), row, 0);

        m_microphoneComboBox = new QComboBox(soundConfigGroupBox);
        configLayout->addWidget(m_microphoneComboBox, row, 1);
    }

    {
        QGroupBox *inputConfigGroupBox = new QGroupBox(tr("Input"), configSection);
        configOuterLayout->addWidget(inputConfigGroupBox);

        QGridLayout *configLayout = new QGridLayout(inputConfigGroupBox);

        int row = 0;

        configLayout->addWidget(new QLabel(tr("Input: "), inputConfigGroupBox), row, 0);

        m_controllerComboBox = new QComboBox(inputConfigGroupBox);
        configLayout->addWidget(m_controllerComboBox, row, 1);

        row++;

        QPushButton *controllerMappingButton = new QPushButton(tr("Configure"), m_controllerComboBox);
        configLayout->addWidget(controllerMappingButton, row, 0, 1, 2);
    }

    configOuterLayout->addStretch();

    QWidget *displaySection = new QWidget(this);
    displaySection->setMinimumSize(854, 480);
    splitter->addWidget(displaySection);

    populateWirelessInterfaces();
    populateMicrophones();
    populateControllers();

    setWindowTitle(tr("Vanilla"));
}

int IsWireless(const char* ifname, char* protocol)
{
    int sock = -1;
    iwreq pwrq;
    memset(&pwrq, 0, sizeof(pwrq));
    strncpy(pwrq.ifr_name, ifname, IFNAMSIZ);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return 0;
    }

    if (ioctl(sock, SIOCGIWNAME, &pwrq) != -1) {
        if (protocol) strncpy(protocol, pwrq.u.name, IFNAMSIZ);
        close(sock);
        return 1;
    }

    close(sock);
    return 0;
}

void Vanilla::populateWirelessInterfaces()
{
    m_wirelessInterfaceComboBox->clear();
    struct ifaddrs *addrs,*tmp;

    getifaddrs(&addrs);
    tmp = addrs;

    while (tmp)
    {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET) {
            if (IsWireless(tmp->ifa_name, nullptr))
                m_wirelessInterfaceComboBox->addItem(tmp->ifa_name);
        }

        tmp = tmp->ifa_next;
    }

    freeifaddrs(addrs);
}

void Vanilla::populateMicrophones()
{
    m_microphoneComboBox->clear();

    auto inputs = QMediaDevices::audioInputs();
    for (auto input : inputs) {
        m_microphoneComboBox->addItem(input.description(), input.id());
    }
}

void Vanilla::populateControllers()
{
    m_controllerComboBox->clear();

    m_controllerComboBox->addItem(tr("Keyboard"), -1);

    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        SDL_Joystick *j = SDL_JoystickOpen(i);
        if (j) {
            m_controllerComboBox->addItem(SDL_JoystickName(j), i);
            SDL_JoystickClose(j);
        }
    }
}

void Vanilla::showSyncDialog()
{
    SyncDialog *d = new SyncDialog(this);
    d->setup(m_wirelessInterfaceComboBox->currentText());
    connect(d, &SyncDialog::finished, d, &SyncDialog::deleteLater);
    d->open();
}