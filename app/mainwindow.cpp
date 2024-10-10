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
#include <QScrollArea>
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

#include "backendinitdialog.h"
#include "inputconfigdialog.h"
#include "keymap.h"
#include "syncdialog.h"
#include "udpaddressdialog.h"

MainWindow::MainWindow(QWidget *parent) : QWidget(parent)
{
    if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
        QMessageBox::critical(this, tr("SDL2 Error"), tr("SDL2 failed to initialize. Controller support will be unavailable."));
    }

    m_backend = nullptr;

    qRegisterMetaType<uint16_t>("uint16_t");

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_splitter = new QSplitter(this);
    layout->addWidget(m_splitter);

    QScrollArea *configScrollArea = new QScrollArea(this);
    configScrollArea->setWidgetResizable(true);
    configScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    configScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_splitter->addWidget(configScrollArea);

    QWidget *configSection = new QWidget(configScrollArea);
    configScrollArea->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    configScrollArea->setWidget(configSection);

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
        connect(m_wirelessInterfaceComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::closeBackend);
        configLayout->addWidget(m_wirelessInterfaceComboBox, row, 1);

        row++;

        m_syncBtn = new QPushButton(tr("Sync"), connectionConfigGroupBox);
        connect(m_syncBtn, &QPushButton::clicked, this, &MainWindow::showSyncDialog);
        configLayout->addWidget(m_syncBtn, row, 0, 1, 2);

        row++;

        m_connectBtn = new QPushButton(connectionConfigGroupBox);
        m_connectBtn->setCheckable(true);
        //m_connectBtn->setEnabled(vanilla_has_config()); // TODO: Implement this properly through the pipe at some point
        setConnectedState(false);
        connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::setConnectedState);
        configLayout->addWidget(m_connectBtn, row, 0, 1, 2);
    }

    {
        QGroupBox *settingsGroupBox = new QGroupBox(tr("Settings"), configSection);
        configOuterLayout->addWidget(settingsGroupBox);

        QGridLayout *configLayout = new QGridLayout(settingsGroupBox);

        int row = 0;

        configLayout->addWidget(new QLabel(tr("Region: "), settingsGroupBox), row, 0);

        m_regionComboBox = new QComboBox(settingsGroupBox);

        m_regionComboBox->addItem(tr("Japan"), VANILLA_REGION_JAPAN);
        m_regionComboBox->addItem(tr("North America"), VANILLA_REGION_AMERICA);
        m_regionComboBox->addItem(tr("Europe"), VANILLA_REGION_EUROPE);
        m_regionComboBox->addItem(tr("China (Unused)"), VANILLA_REGION_CHINA);
        m_regionComboBox->addItem(tr("South Korea (Unused)"), VANILLA_REGION_SOUTH_KOREA);
        m_regionComboBox->addItem(tr("Taiwan (Unused)"), VANILLA_REGION_TAIWAN);
        m_regionComboBox->addItem(tr("Australia (Unused)"), VANILLA_REGION_AUSTRALIA);

        // TODO: Should probably save/load this from a config file
        m_regionComboBox->setCurrentIndex(VANILLA_REGION_AMERICA);

        connect(m_regionComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::updateRegionFromComboBox);

        configLayout->addWidget(m_regionComboBox, row, 1);

        row++;

        configLayout->addWidget(new QLabel(tr("Battery Status: "), settingsGroupBox), row, 0);

        m_batteryStatusComboBox = new QComboBox(settingsGroupBox);

        m_batteryStatusComboBox->addItem(tr("Charging"), VANILLA_BATTERY_STATUS_CHARGING);
        m_batteryStatusComboBox->addItem(tr("Unknown"), VANILLA_BATTERY_STATUS_UNKNOWN);
        m_batteryStatusComboBox->addItem(tr("Very Low"), VANILLA_BATTERY_STATUS_VERY_LOW);
        m_batteryStatusComboBox->addItem(tr("Low"), VANILLA_BATTERY_STATUS_LOW);
        m_batteryStatusComboBox->addItem(tr("Medium"), VANILLA_BATTERY_STATUS_MEDIUM);
        m_batteryStatusComboBox->addItem(tr("High"), VANILLA_BATTERY_STATUS_HIGH);
        m_batteryStatusComboBox->addItem(tr("Full"), VANILLA_BATTERY_STATUS_FULL);

        connect(m_batteryStatusComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::updateBatteryStatus);

        configLayout->addWidget(m_batteryStatusComboBox, row, 1);
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
        m_microphoneComboBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        configLayout->addWidget(m_microphoneComboBox, row, 1);
    }

    {
        QGroupBox *inputConfigGroupBox = new QGroupBox(tr("Input"), configSection);
        configOuterLayout->addWidget(inputConfigGroupBox);

        QGridLayout *configLayout = new QGridLayout(inputConfigGroupBox);

        int row = 0;

        configLayout->addWidget(new QLabel(tr("Input: "), inputConfigGroupBox), row, 0);

        m_controllerComboBox = new QComboBox(inputConfigGroupBox);
        m_controllerComboBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        connect(m_controllerComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::setJoystick);
        configLayout->addWidget(m_controllerComboBox, row, 1);

        row++;

        m_controllerMappingButton = new QPushButton(tr("Configure"), m_controllerComboBox);
        connect(m_controllerMappingButton, &QPushButton::clicked, this, &MainWindow::showInputConfigDialog);
        configLayout->addWidget(m_controllerMappingButton, row, 0, 1, 2);
    }

    configOuterLayout->addStretch();

    m_videoDecoder = new VideoDecoder();
    connect(m_recordBtn, &QPushButton::clicked, m_videoDecoder, &VideoDecoder::enableRecording);
    startObjectOnThread(m_videoDecoder);

    m_gamepadHandler = new GamepadHandler();
    startObjectOnThread(m_gamepadHandler);
    QMetaObject::invokeMethod(m_gamepadHandler, &GamepadHandler::run, Qt::QueuedConnection);

    m_audioHandler = new AudioHandler();
    startObjectOnThread(m_audioHandler);
    QMetaObject::invokeMethod(m_audioHandler, &AudioHandler::run, Qt::QueuedConnection);

    connect(m_videoDecoder, &VideoDecoder::frameReady, m_viewer, &Viewer::setImage);
    connect(m_videoDecoder, &VideoDecoder::recordingError, this, &MainWindow::recordingError);
    connect(m_videoDecoder, &VideoDecoder::recordingFinished, this, &MainWindow::recordingFinished);
    connect(m_gamepadHandler, &GamepadHandler::gamepadsChanged, this, &MainWindow::populateControllers);
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

    closeBackend();

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
            if (isWireless(tmp->ifa_name, nullptr)) {
                QString s = tmp->ifa_name;
                m_wirelessInterfaceComboBox->addItem(s, s);
            }
        }

        tmp = tmp->ifa_next;
    }

    freeifaddrs(addrs);

    if (m_wirelessInterfaceComboBox->count() > 0) {
        m_wirelessInterfaceComboBox->insertSeparator(m_wirelessInterfaceComboBox->count());
    }

    m_wirelessInterfaceComboBox->addItem(tr("External Server..."));
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

template<typename T>
void MainWindow::initBackend(T func)
{
    if (!m_backend) {
        BackendInitDialog *d = new BackendInitDialog(this);
        d->open();

        QString localWirelessIntf = m_wirelessInterfaceComboBox->currentData().toString();
        if (localWirelessIntf.isEmpty()) {
            UdpAddressDialog udpDiag(this);
            if (udpDiag.exec() == QDialog::Accepted) {
                m_backend = new BackendViaExternalPipe(udpDiag.acceptedAddress());
            } else {
                d->deleteLater();
                closeBackend();
                return;
            }
        // } else if (geteuid() == 0) {
            // If root, use lib locally
            // m_backend = new Backend(QHostAddress());
        } else {
            m_backend = new BackendViaInternalPipe(localWirelessIntf);
        }

        connect(m_backend, &Backend::closed, d, &BackendInitDialog::deleteLater);
        connect(m_backend, &Backend::ready, d, &BackendInitDialog::deleteLater);

        connect(m_backend, &Backend::closed, this, &MainWindow::closeBackend);
        connect(m_backend, &Backend::ready, this, func);
        connect(m_backend, &Backend::error, this, &MainWindow::showBackendError);

        connect(m_backend, &Backend::videoAvailable, m_videoDecoder, &VideoDecoder::sendPacket);
        connect(m_backend, &Backend::audioAvailable, m_videoDecoder, &VideoDecoder::sendAudio);
        connect(m_backend, &Backend::syncCompleted, this, [this](bool e){if (e) m_connectBtn->setEnabled(true);});
        connect(m_videoDecoder, &VideoDecoder::requestIDR, m_backend, &Backend::requestIDR, Qt::DirectConnection);
        connect(m_backend, &Backend::audioAvailable, m_audioHandler, &AudioHandler::write);
        connect(m_backend, &Backend::vibrate, m_gamepadHandler, &GamepadHandler::vibrate, Qt::DirectConnection);
        connect(m_viewer, &Viewer::touch, m_backend, &Backend::updateTouch, Qt::DirectConnection);
        connect(m_gamepadHandler, &GamepadHandler::buttonStateChanged, m_backend, &Backend::setButton, Qt::DirectConnection);

        startObjectOnThread(m_backend);
        QMetaObject::invokeMethod(m_backend, &Backend::init, Qt::QueuedConnection);
    } else {
        func();
    }
}

void MainWindow::showSyncDialog()
{
    initBackend([this]{
        SyncDialog *d = new SyncDialog(this);
        d->setup(m_backend);
        connect(d, &SyncDialog::finished, d, &SyncDialog::deleteLater);
        d->open();
    });
}

void MainWindow::setConnectedState(bool on)
{
    m_wirelessInterfaceComboBox->setEnabled(!on);
    m_syncBtn->setEnabled(!on);
    m_connectBtn->setChecked(on);
    m_connectBtn->setText(on ? tr("Disconnect") : tr("Connect"));
    if (on) {
        initBackend([this]{
            QMetaObject::invokeMethod(m_backend, &Backend::connectToConsole, Qt::QueuedConnection);

            updateVolumeAxis();
            updateRegion();
            updateBatteryStatus();
        });
    } else {
        if (m_backend) {
            m_backend->interrupt();
        }

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
    if (m_backend) m_backend->setButton(VANILLA_AXIS_VOLUME, m_volumeSlider->value() * 0xFF / m_volumeSlider->maximum());
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
    while (1) {
        QString s = QFileDialog::getSaveFileName(this, tr("Save Screenshot"), QString(), tr("MPEG-4 Video (*.mp4)"));
        if (s.isEmpty()) {
            break;
        }

        QString ext = QStringLiteral(".mp4");
        if (!s.endsWith(ext, Qt::CaseInsensitive)) {
            s = s.append(ext);
        }

        if ((!QFile::exists(s) || QFile::remove(s)) && QFile::copy(filename, s)) {
            // Copied file successfully
            break;
        }

        // Failed to write file
        QMessageBox::warning(this, tr("Save Failed"), tr("Failed to save to location '%0'. Please try another location.").arg(s));
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

void MainWindow::updateRegionFromComboBox()
{
    updateRegion();
    if (m_connectBtn->isChecked()) {
        QMessageBox::information(this, tr("Region Change"), tr("Changes will take effect after disconnecting and reconnecting."));
    }
}

void MainWindow::updateRegion()
{
    if (m_backend) m_backend->setRegion(m_regionComboBox->currentData().toInt());
}

void MainWindow::updateBatteryStatus()
{
    if (m_backend) m_backend->setBatteryStatus(m_batteryStatusComboBox->currentData().toInt());
}

void MainWindow::closeBackend()
{
    setConnectedState(false);

    if (m_backend) {
        m_backend->interrupt();
        m_backend->deleteLater();
        m_backend = nullptr;
    }
}

void MainWindow::showBackendError(const QString &err)
{
    QMessageBox::critical(this, tr("Backend Error"), err);
}