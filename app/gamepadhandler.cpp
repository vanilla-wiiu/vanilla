#include "gamepadhandler.h"

#include <QDateTime>
#include <vanilla.h>

#include "keymap.h"

static int g_buttonMap[SDL_CONTROLLER_BUTTON_MAX] = {-1};
static int g_axisMap[SDL_CONTROLLER_AXIS_MAX] = {-1};

GamepadHandler::GamepadHandler(QObject *parent) : QObject(parent)
{
    m_closed = false;
    m_controller = nullptr;
    m_nextGamepad = -2;
    m_vibrate = false;

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
    g_axisMap[SDL_CONTROLLER_AXIS_TRIGGERLEFT] = VANILLA_BTN_ZL;
    g_axisMap[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] = VANILLA_BTN_ZR;
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

void EnableSensorIfAvailable(SDL_GameController *controller, SDL_SensorType sensor)
{
    if (SDL_GameControllerHasSensor(controller, sensor)) {
        SDL_GameControllerSetSensorEnabled(controller, sensor, SDL_TRUE);
    }
}

int32_t packFloat(float f)
{
    int32_t x;
    memcpy(&x, &f, sizeof(int32_t));
    return x;
}

void GamepadHandler::run()
{
    m_mutex.lock();

    while (!m_closed) {// See status of rumble
        if (m_nextGamepad != -2) {
            if (m_controller) {
                SDL_GameControllerClose(m_controller);
            }
            if (m_nextGamepad != -1) {
                m_controller = SDL_GameControllerOpen(m_nextGamepad);
                EnableSensorIfAvailable(m_controller, SDL_SENSOR_ACCEL);
                EnableSensorIfAvailable(m_controller, SDL_SENSOR_GYRO);
            } else {
                m_controller = nullptr;
            }
            m_nextGamepad = -2;
        }
    
        if (m_controller) {
            uint16_t amount = m_vibrate ? 0xFFFF : 0;
            SDL_GameControllerRumble(m_controller, amount, amount, 0);
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
                    if (vanilla_btn != -1) {
                        emit buttonStateChanged(vanilla_btn, event.type == SDL_CONTROLLERBUTTONDOWN ? INT16_MAX : 0);
                    }
                }
                break;
            case SDL_CONTROLLERAXISMOTION:
                if (m_controller && event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(m_controller))) {
                    int vanilla_axis = g_axisMap[event.caxis.axis];
                    Sint16 axis_value = event.caxis.value;
                    if (vanilla_axis != -1) {
                        emit buttonStateChanged(vanilla_axis, axis_value);
                    }
                }
                break;
            case SDL_CONTROLLERSENSORUPDATE:
                if (event.csensor.sensor == SDL_SENSOR_ACCEL) {
                    emit buttonStateChanged(VANILLA_SENSOR_ACCEL_X, packFloat(event.csensor.data[0]));
                    emit buttonStateChanged(VANILLA_SENSOR_ACCEL_Y, packFloat(event.csensor.data[1]));
                    emit buttonStateChanged(VANILLA_SENSOR_ACCEL_Z, packFloat(event.csensor.data[2]));
                } else if (event.csensor.sensor == SDL_SENSOR_GYRO) {
                    emit buttonStateChanged(VANILLA_SENSOR_GYRO_PITCH, packFloat(event.csensor.data[0]));
                    emit buttonStateChanged(VANILLA_SENSOR_GYRO_YAW, packFloat(event.csensor.data[1]));
                    emit buttonStateChanged(VANILLA_SENSOR_GYRO_ROLL, packFloat(event.csensor.data[2]));
                }
                break;
            }
        }

        m_mutex.lock();
    }

    m_mutex.unlock();
}

void GamepadHandler::vibrate(bool on)
{
    m_mutex.lock();
    m_vibrate = on;
    m_mutex.unlock();
}

void GamepadHandler::keyPressed(Qt::Key key)
{
    if (!m_controller) {
        auto it = KeyMap::instance.find(key);
        if (it != KeyMap::instance.end()) {
            emit buttonStateChanged(it->second, INT16_MAX);
        }
    }
}

void GamepadHandler::keyReleased(Qt::Key key)
{
    if (!m_controller) {
        auto it = KeyMap::instance.find(key);
        if (it != KeyMap::instance.end()) {
            emit buttonStateChanged(it->second, 0);
        }
    }
}