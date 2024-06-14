#include "mainwindow.h"

#include <QAudioDevice>
#include <QComboBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaDevices>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
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

#include "inputconfigdialog.h"
#include "keymap.h"
#include "syncdialog.h"

MainWindow::MainWindow(QWidget *parent) : QWidget(parent)
{
    if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
        QMessageBox::critical(this, tr("SDL2 Error"), tr("SDL2 failed to initialize. Controller support will be unavailable."));
    }

    qRegisterMetaType<uint16_t>("uint16_t");

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_splitter = new QSplitter(this);
    layout->addWidget(m_splitter);

    QWidget *configSection = new QWidget(this);
    m_splitter->addWidget(configSection);

    m_viewer = new Viewer(this);
    m_viewer->setMinimumSize(854, 480);
    connect(m_viewer, &Viewer::requestExitFullScreen, this, &MainWindow::exitFullScreen);
    m_splitter->addWidget(m_viewer);

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
        //m_connectBtn->setEnabled(vanilla_has_config()); // TODO: Implement this properly through the pipe at some point
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

        QPushButton *fullScreenBtn = new QPushButton(tr("Full Screen"), configSection);
        connect(fullScreenBtn, &QPushButton::clicked, this, &MainWindow::setFullScreen);
        configLayout->addWidget(fullScreenBtn, row, 0, 1, 2);
        
        row++;

        m_recordBtn = new QPushButton(tr("Record"), configSection);
        m_recordBtn->setCheckable(true);
        configLayout->addWidget(m_recordBtn, row, 0);

        m_screenshotBtn = new QPushButton(tr("Screenshot"), configSection);
        connect(m_screenshotBtn, &QPushButton::clicked, this, &MainWindow::takeScreenshot);
        configLayout->addWidget(m_screenshotBtn, row, 1);
    }

    {
        QGroupBox *soundConfigGroupBox = new QGroupBox(tr("Sound"), configSection);
        configOuterLayout->addWidget(soundConfigGroupBox);

        QGridLayout *configLayout = new QGridLayout(soundConfigGroupBox);

        int row = 0;

        configLayout->addWidget(new QLabel(tr("Volume: "), soundConfigGroupBox), row, 0);
        
        m_volumeSlider = new QSlider(Qt::Horizontal, soundConfigGroupBox);
        m_volumeSlider->setMinimum(0);
        m_volumeSlider->setMaximum(100);
        m_volumeSlider->setValue(100);
        connect(m_volumeSlider, &QSlider::valueChanged, this, &MainWindow::volumeChanged);
        configLayout->addWidget(m_volumeSlider, row, 1);

        row++;

        configLayout->addWidget(new QLabel(tr("Microphone: "), soundConfigGroupBox), row, 0);

        m_microphoneComboBox = new QComboBox(soundConfigGroupBox);
        m_microphoneComboBox->setMaximumWidth(m_microphoneComboBox->fontMetrics().horizontalAdvance(QStringLiteral("SOMEAMOUNTOFTEXT")));
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

        m_controllerMappingButton = new QPushButton(tr("Configure"), m_controllerComboBox);
        connect(m_controllerMappingButton, &QPushButton::clicked, this, &MainWindow::showInputConfigDialog);
        configLayout->addWidget(m_controllerMappingButton, row, 0, 1, 2);
    }

    configOuterLayout->addStretch();

    m_backend = new Backend();
    startObjectOnThread(m_backend);

    m_videoDecoder = new VideoDecoder();
    connect(m_recordBtn, &QPushButton::clicked, m_videoDecoder, &VideoDecoder::enableRecording);
    startObjectOnThread(m_videoDecoder);

    m_gamepadHandler = new GamepadHandler();
    startObjectOnThread(m_gamepadHandler);
    QMetaObject::invokeMethod(m_gamepadHandler, &GamepadHandler::run, Qt::QueuedConnection);

    m_audioHandler = new AudioHandler();
    startObjectOnThread(m_audioHandler);
    QMetaObject::invokeMethod(m_audioHandler, &AudioHandler::run, Qt::QueuedConnection);

    connect(m_backend, &Backend::videoAvailable, m_videoDecoder, &VideoDecoder::sendPacket);
    connect(m_backend, &Backend::audioAvailable, m_videoDecoder, &VideoDecoder::sendAudio);
    connect(m_backend, &Backend::syncCompleted, this, [this](bool e){if (e) m_connectBtn->setEnabled(true);});
    connect(m_videoDecoder, &VideoDecoder::frameReady, m_viewer, &Viewer::setImage);
    connect(m_videoDecoder, &VideoDecoder::recordingError, this, &MainWindow::recordingError);
    connect(m_videoDecoder, &VideoDecoder::recordingFinished, this, &MainWindow::recordingFinished);
    connect(m_videoDecoder, &VideoDecoder::requestIDR, m_backend, &Backend::requestIDR, Qt::DirectConnection);
    connect(m_backend, &Backend::audioAvailable, m_audioHandler, &AudioHandler::write);
    connect(m_backend, &Backend::vibrate, m_gamepadHandler, &GamepadHandler::vibrate, Qt::DirectConnection);
    connect(m_viewer, &Viewer::touch, m_backend, &Backend::updateTouch, Qt::DirectConnection);
    connect(m_gamepadHandler, &GamepadHandler::gamepadsChanged, this, &MainWindow::populateControllers);
    connect(m_gamepadHandler, &GamepadHandler::buttonStateChanged, m_backend, &Backend::setButton, Qt::DirectConnection);
    connect(m_viewer, &Viewer::keyPressed, m_gamepadHandler, &GamepadHandler::keyPressed, Qt::DirectConnection);
    connect(m_viewer, &Viewer::keyReleased, m_gamepadHandler, &GamepadHandler::keyReleased, Qt::DirectConnection);

    populateWirelessInterfaces();
    populateMicrophones();
    populateControllers();

    KeyMap::instance.load(KeyMap::getConfigFilename());

    setWindowTitle(tr("Vanilla"));
}

MainWindow::~MainWindow()
{
    QMetaObject::invokeMethod(m_audioHandler, &AudioHandler::close, Qt::QueuedConnection);
    m_audioHandler->deleteLater();

    m_gamepadHandler->close();
    m_gamepadHandler->deleteLater();

    m_videoDecoder->deleteLater();

    m_backend->interrupt();
    m_backend->deleteLater();

    for (QThread *t : m_threadMap) {
        t->quit();
        t->wait();
        delete t;
    }

    SDL_Quit();

    delete m_viewer;
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
    d->setup(m_backend, m_wirelessInterfaceComboBox->currentText());
    connect(d, &SyncDialog::finished, d, &SyncDialog::deleteLater);
    d->open();
}

void MainWindow::setConnectedState(bool on)
{
    m_wirelessInterfaceComboBox->setEnabled(!on);
    m_syncBtn->setEnabled(!on);
    if (on) {
        m_connectBtn->setText(tr("Disconnect"));

        QMetaObject::invokeMethod(m_backend, "connectToConsole", Qt::QueuedConnection, Q_ARG(QString, m_wirelessInterfaceComboBox->currentText()));

        updateVolumeAxis();
    } else {
        if (m_backend) {
            m_backend->interrupt();
        }

        m_connectBtn->setText(tr("Connect"));

        m_viewer->setImage(QImage());
    }
}

void MainWindow::setJoystick(int index)
{
    m_controllerMappingButton->setEnabled(index == 0); // Currently we only allow configuring keyboard controls
    m_gamepadHandler->setController(index - 1);
}

void MainWindow::setFullScreen()
{
    m_viewer->setParent(nullptr);
    m_viewer->showFullScreen();
}

void MainWindow::exitFullScreen()
{
    m_splitter->addWidget(m_viewer);
}

void MainWindow::updateVolumeAxis()
{
    m_backend->setButton(VANILLA_AXIS_VOLUME, m_volumeSlider->value() * 0xFF / m_volumeSlider->maximum());
}

void MainWindow::volumeChanged(int v)
{
    qreal vol = v * 0.01;
    vol = QAudio::convertVolume(vol, QAudio::LinearVolumeScale, QAudio::LogarithmicVolumeScale);
    QMetaObject::invokeMethod(m_audioHandler, "setVolume", Q_ARG(qreal, vol));

    updateVolumeAxis();
}

void MainWindow::showInputConfigDialog()
{
    InputConfigDialog *d = new InputConfigDialog(this);
    d->open();
}

void MainWindow::startObjectOnThread(QObject *object)
{
    QThread *thread = new QThread(this);
    object->moveToThread(thread);
    thread->start();
    m_threadMap.insert(object, thread);
}

void MainWindow::recordingError(int err)
{
    char buf[64] = {0};
    av_make_error_string(buf, sizeof(buf), err);
    QMessageBox::critical(this, tr("Recording Error"), tr("Recording failed with the following error: %0 (%1)").arg(buf, QString::number(err)));
}

void MainWindow::recordingFinished(const QString &filename)
{
    QString s = QFileDialog::getSaveFileName(this, tr("Save Screenshot"), QString(), tr("MPEG-4 Video (*.mp4)"));
    if (!s.isEmpty()) {
        QString ext = QStringLiteral(".mp4");
        if (!s.endsWith(ext, Qt::CaseInsensitive)) {
            s = s.append(ext);
        }

        QFile::copy(filename, s);
    }
}

void MainWindow::takeScreenshot()
{
    // Make a copy of the current image
    QImage ss = m_viewer->image();

    QString s = QFileDialog::getSaveFileName(this, tr("Save Screenshot"), QString(), tr("PNG (*.png)"));
    if (!s.isEmpty()) {
        QString ext = QStringLiteral(".png");
        if (!s.endsWith(ext, Qt::CaseInsensitive)) {
            s = s.append(ext);
        }
        ss.save(s);
    }
}