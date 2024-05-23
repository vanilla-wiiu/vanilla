#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QComboBox>
#include <QProcess>
#include <QPushButton>
#include <QSplitter>
#include <QWidget>
#include <SDL2/SDL_gamecontroller.h>

#include "audiohandler.h"
#include "backend.h"
#include "gamepadhandler.h"
#include "videodecoder.h"
#include "viewer.h"

class MainWindow : public QWidget
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    virtual ~MainWindow() override;

    static MainWindow *instance();

public slots:
    void enableConnectButton();

private:
    void populateWirelessInterfaces();
    void populateMicrophones();
    void populateControllers();

    Viewer *m_viewer;

    QComboBox *m_wirelessInterfaceComboBox;
    QComboBox *m_microphoneComboBox;
    QComboBox *m_controllerComboBox;
    QProcess *m_process;

    QPushButton *m_syncBtn;
    QPushButton *m_connectBtn;

    QSplitter *m_splitter;

    Backend *m_backend;
    VideoDecoder *m_videoDecoder;

    QThread *m_backendThread;
    QThread *m_videoDecoderThread;

    GamepadHandler *m_gamepadHandler;
    QThread *m_gamepadHandlerThread;

    AudioHandler *m_audioHandler;
    QThread *m_audioHandlerThread;

    QPushButton *m_controllerMappingButton;

private slots:
    void showSyncDialog();

    void setConnectedState(bool on);

    void setJoystick(int index);

    void setFullScreen();
    void exitFullScreen();

    void volumeChanged(int val);

    void showInputConfigDialog();

};

#endif // MAINWINDOW_H
