#ifndef GAMEPAD_HANDLER_H
#define GAMEPAD_HANDLER_H

#include <QMutex>
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

    void vibrate(qint64 duration);

private:
    QMutex m_mutex;

    bool m_closed;
    int m_nextGamepad;
    qint64 m_rumbleEnd;

    SDL_GameController *m_controller;

};

#endif // GAMEPAD_HANDLER_H