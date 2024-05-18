#include "gamepadhandler.h"

#include <QDateTime>
#include <vanilla.h>

static int g_buttonMap[SDL_CONTROLLER_BUTTON_MAX] = {0};
static int g_axisMap[SDL_CONTROLLER_AXIS_MAX] = {0};

GamepadHandler::GamepadHandler(QObject *parent) : QObject(parent)
{
    m_closed = false;
    m_controller = nullptr;
    m_nextGamepad = -1;
    m_rumbleEnd = 0;

    g_buttonMap[SDL_CONTROLLER_BUTTON_A] = VANILLA_BTN_A;
    g_buttonMap[SDL_CONTROLLER_BUTTON_B] = VANILLA_BTN_B;
    g_buttonMap[SDL_CONTROLLER_BUTTON_X] = VANILLA_BTN_X;
    g_buttonMap[SDL_CONTROLLER_BUTTON_Y] = VANILLA_BTN_Y;
    g_buttonMap[SDL_CONTROLLER_BUTTON_BACK] = VANILLA_BTN_MINUS;
    g_buttonMap[SDL_CONTROLLER_BUTTON_GUIDE] = VANILLA_BTN_HOME;
    g_buttonMap[SDL_CONTROLLER_BUTTON_START] = VANILLA_BTN_PLUS;
    g_buttonMap[SDL_CONTROLLER_BUTTON_LEFTSTICK] = VANILLA_BTN_L3;
    g_buttonMap[SDL_CONTROLLER_BUTTON_RIGHTSTICK] = VANILLA_BTN_R3;
    g_buttonMap[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] = VANILLA_BTN_L;
    g_buttonMap[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = VANILLA_BTN_R;
    g_buttonMap[SDL_CONTROLLER_BUTTON_DPAD_UP] = VANILLA_BTN_UP;
    g_buttonMap[SDL_CONTROLLER_BUTTON_DPAD_DOWN] = VANILLA_BTN_DOWN;
    g_buttonMap[SDL_CONTROLLER_BUTTON_DPAD_LEFT] = VANILLA_BTN_LEFT;
    g_buttonMap[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] = VANILLA_BTN_RIGHT;

    g_axisMap[SDL_CONTROLLER_AXIS_LEFTX] = VANILLA_AXIS_L_X;
    g_axisMap[SDL_CONTROLLER_AXIS_LEFTY] = VANILLA_AXIS_L_Y;
    g_axisMap[SDL_CONTROLLER_AXIS_RIGHTX] = VANILLA_AXIS_R_X;
    g_axisMap[SDL_CONTROLLER_AXIS_RIGHTY] = VANILLA_AXIS_R_Y;
}

void GamepadHandler::close()
{
    m_mutex.lock();
    m_closed = true;
    m_mutex.unlock();
}

void GamepadHandler::setController(int index)
{
    m_mutex.lock();
    m_nextGamepad = index;
    m_mutex.unlock();
}

float transformAxisValue(int16_t val)
{
    if (val < 0) {
        return val / 32768.0f;
    } else {
        return val / 32767.0f;
    }
}

void GamepadHandler::run()
{
    m_mutex.lock();

    while (!m_closed) {// See status of rumble
        if (m_nextGamepad != -1) {
            if (m_controller) {
                SDL_GameControllerClose(m_controller);
            }
            m_controller = SDL_GameControllerOpen(m_nextGamepad);
            m_nextGamepad = -1;
        }
    
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_rumbleEnd > now && m_controller) {
            SDL_GameControllerRumble(m_controller, 0xFFFF, 0xFFFF, m_rumbleEnd - now);
        }

        m_mutex.unlock();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_CONTROLLERDEVICEADDED:
                emit gamepadsChanged();
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                // Our connected controller was disconnected
                if (m_controller && event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(m_controller))) {
                    SDL_GameControllerClose(m_controller);
                    m_controller = nullptr;
                }
                emit gamepadsChanged();
                break;
            case SDL_CONTROLLERDEVICEREMAPPED:
                break;
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
                if (m_controller && event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(m_controller))) {
                    int vanilla_btn = g_buttonMap[event.cbutton.button];
                    if (vanilla_btn) {
                        vanilla_set_button(vanilla_btn, event.type == SDL_CONTROLLERBUTTONDOWN);
                    }
                }
                break;
            case SDL_CONTROLLERAXISMOTION:
                if (m_controller && event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(m_controller))) {
                    int vanilla_axis = g_axisMap[event.caxis.axis];
                    Sint16 axis_value = event.caxis.value;
                    if (vanilla_axis) {
                        vanilla_set_axis(vanilla_axis, transformAxisValue(axis_value));
                    } else if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) {
                        vanilla_set_button(VANILLA_BTN_ZL, axis_value > 0);
                    } else if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
                        vanilla_set_button(VANILLA_BTN_ZR, axis_value > 0);
                    }
                }
                break;
            }
        }


        m_mutex.lock();
    }

    m_mutex.unlock();
}

void GamepadHandler::vibrate(qint64 duration)
{
    qint64 rumbleEnd = QDateTime::currentMSecsSinceEpoch() + duration;

    m_mutex.lock();
    m_rumbleEnd = rumbleEnd;
    m_mutex.unlock();
}