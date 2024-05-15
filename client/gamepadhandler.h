#ifndef GAMEPAD_HANDLER_H
#define GAMEPAD_HANDLER_H

#include <QObject>
#include <SDL2/SDL.h>

class GamepadHandler : public QObject
{
    Q_OBJECT
public:
    GamepadHandler(QObject *parent = nullptr);

    void close();

    void setController(int index);

signals:
    void gamepadsChanged();

public slots:
    void run();

private:
    QAtomicInt m_closed;
    QAtomicInt m_nextGamepad;

    SDL_GameController *m_controller;

};

#endif // GAMEPAD_HANDLER_H