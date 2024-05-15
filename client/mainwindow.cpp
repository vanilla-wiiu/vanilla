#include "mainwindow.h"

#include <QAudioDevice>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaDevices>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QThread>
#include <SDL2/SDL.h>

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/wireless.h>
#include <vanilla.h>

#include "syncdialog.h"

MainWindow::MainWindow(QWidget *parent) : QWidget(parent)
{
    if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
        QMessageBox::critical(this, tr("SDL2 Error"), tr("SDL2 failed to initialize. Controller support will be unavailable."));
    }

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

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

        m_syncBtn = new QPushButton(tr("Sync"), connectionConfigGroupBox);
        connect(m_syncBtn, &QPushButton::clicked, this, &MainWindow::showSyncDialog);
        configLayout->addWidget(m_syncBtn, row, 0, 1, 2);

        row++;

        m_connectBtn = new QPushButton(connectionConfigGroupBox);
        m_connectBtn->setCheckable(true);
        m_backend = nullptr;
        setConnectedState(false);
        connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::setConnectedState);
        configLayout->addWidget(m_connectBtn, row, 0, 1, 2);
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
        connect(m_controllerComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::setJoystick);
        configLayout->addWidget(m_controllerComboBox, row, 1);

        row++;

        QPushButton *controllerMappingButton = new QPushButton(tr("Configure"), m_controllerComboBox);
        configLayout->addWidget(controllerMappingButton, row, 0, 1, 2);
    }

    configOuterLayout->addStretch();

    m_viewer = new Viewer(this);
    m_viewer->setMinimumSize(848, 480);
    splitter->addWidget(m_viewer);

    m_gamepadHandler = new GamepadHandler();
    m_gamepadHandlerThread = new QThread(this);
    m_gamepadHandler->moveToThread(m_gamepadHandlerThread);
    m_gamepadHandlerThread->start();
    connect(m_gamepadHandler, &GamepadHandler::gamepadsChanged, this, &MainWindow::populateControllers);
    QMetaObject::invokeMethod(m_gamepadHandler, &GamepadHandler::run, Qt::QueuedConnection);

    populateWirelessInterfaces();
    populateMicrophones();
    populateControllers();

    setWindowTitle(tr("Vanilla"));
}

MainWindow::~MainWindow()
{
    m_gamepadHandler->close();
    m_gamepadHandlerThread->quit();
    m_gamepadHandlerThread->wait();
    delete m_gamepadHandler;
    delete m_gamepadHandlerThread;

    SDL_Quit();

    setConnectedState(false);
}

int isWireless(const char* ifname, char* protocol)
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

void MainWindow::populateWirelessInterfaces()
{
    m_wirelessInterfaceComboBox->clear();
    struct ifaddrs *addrs,*tmp;

    getifaddrs(&addrs);
    tmp = addrs;

    while (tmp)
    {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET) {
            if (isWireless(tmp->ifa_name, nullptr))
                m_wirelessInterfaceComboBox->addItem(tmp->ifa_name);
        }

        tmp = tmp->ifa_next;
    }

    freeifaddrs(addrs);
}

void MainWindow::populateMicrophones()
{
    m_microphoneComboBox->clear();

    auto inputs = QMediaDevices::audioInputs();
    for (auto input : inputs) {
        m_microphoneComboBox->addItem(input.description(), input.id());
    }
}

void MainWindow::populateControllers()
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

void MainWindow::showSyncDialog()
{
    SyncDialog *d = new SyncDialog(this);
    d->setup(m_wirelessInterfaceComboBox->currentText());
    connect(d, &SyncDialog::finished, d, &SyncDialog::deleteLater);
    d->open();
}

void MainWindow::setConnectedState(bool on)
{
    if (on) {
        m_connectBtn->setText(tr("Disconnect"));

        m_backendThread = new QThread(this);
        m_videoDecoderThread = new QThread(this);
        
        m_backend = new Backend();
        m_videoDecoder = new VideoDecoder();

        connect(m_backend, &Backend::videoAvailable, m_videoDecoder, &VideoDecoder::sendPacket);
        connect(m_videoDecoder, &VideoDecoder::frameReady, m_viewer, &Viewer::setImage);

        m_backend->moveToThread(m_backendThread);
        m_videoDecoder->moveToThread(m_videoDecoderThread);

        m_videoDecoderThread->start();
        m_backendThread->start();

        QMetaObject::invokeMethod(m_backend, &Backend::connectToConsole, Qt::QueuedConnection, m_wirelessInterfaceComboBox->currentText());
    } else {
        m_connectBtn->setText(tr("Connect"));

        if (m_backend) {
            m_backend->interrupt();

            m_backend->deleteLater();
            m_backend = nullptr;

            m_videoDecoderThread->deleteLater();
            m_videoDecoder = nullptr;

            m_backendThread->quit();
            m_videoDecoderThread->quit();

            m_backendThread->wait();
            m_videoDecoderThread->wait();

            m_backendThread->deleteLater();
            m_videoDecoderThread->deleteLater();
        }
    }
}

void MainWindow::setJoystick(int index)
{
    if (index == 0) {
        // Keyboard
        return;
    }

    m_gamepadHandler->setController(index - 1);
}