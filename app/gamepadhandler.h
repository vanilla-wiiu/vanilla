#ifndef GAMEPAD_HANDLER_H
#define GAMEPAD_HANDLER_H

#include <QMap>
#include <QMutex>
#include <QObject>
#include <SDL2/SDL.h>

#include "keymap.h"

class GamepadHandler : public QObject
{
    Q_OBJECT
public:
    GamepadHandler(QObject *parent = nullptr);

    void close();

    void setController(int index);

    void setButton(SDL_GameControllerButton sdl_btn, int vanilla_btn);
    void setAxis(SDL_GameControllerAxis sdl_axis, int vanilla_axis);
    void clear();
    void save();

    int button(SDL_GameControllerButton sdl_btn);
    int axis(SDL_GameControllerAxis sdl_axis);

    void setKeyMap(KeyMap *keyMap);

    void signalNextButtonOrAxis(bool e);

signals:
    void gamepadsChanged();

    void buttonStateChanged(int button, int32_t value);

    void buttonPressed(SDL_GameControllerButton button);
    void axisMoved(SDL_GameControllerAxis axis);

public slots:
    void run();

    void vibrate(bool on);

    void keyPressed(Qt::Key key);
    void keyReleased(Qt::Key key);

private:
    void saveCustomProfile();
    static QString getConfigFilename(SDL_JoystickGUID guid);

    QMutex m_mutex;

    bool m_closed;
    int m_nextGamepad;
    bool m_vibrate;
    bool m_signalNext;

    SDL_GameController *m_controller;

    QMutex m_mapMutex;
    int m_buttonMap[SDL_CONTROLLER_BUTTON_MAX];
    int m_axisMap[SDL_CONTROLLER_AXIS_MAX];

    KeyMap *m_keyMap;

};

#endif // GAMEPAD_HANDLER_H