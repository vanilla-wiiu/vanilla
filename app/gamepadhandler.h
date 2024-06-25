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

    void buttonStateChanged(int button, int32_t value);

public slots:
    void run();

    void vibrate(bool on);

    void keyPressed(Qt::Key key);
    void keyReleased(Qt::Key key);

private:
    QMutex m_mutex;

    bool m_closed;
    int m_nextGamepad;
    bool m_vibrate;

    SDL_GameController *m_controller;

};

#endif // GAMEPAD_HANDLER_H