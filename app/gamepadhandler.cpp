#include "gamepadhandler.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QXmlStreamWriter>
#include <vanilla.h>

static bool g_defaultMapsInitialized = false;
static int g_defaultButtonMap[SDL_CONTROLLER_BUTTON_MAX] = {-1};
static int g_defaultAxisMap[SDL_CONTROLLER_AXIS_MAX] = {-1};

GamepadHandler::GamepadHandler(QObject *parent) : QObject(parent)
{
    m_closed = false;
    m_controller = nullptr;
    m_nextGamepad = -2;
    m_vibrate = false;
    m_keyMap = nullptr;
    m_signalNext = false;

    if (!g_defaultMapsInitialized) {
        g_defaultButtonMap[SDL_CONTROLLER_BUTTON_A] = VANILLA_BTN_A;
        g_defaultButtonMap[SDL_CONTROLLER_BUTTON_B] = VANILLA_BTN_B;
        g_defaultButtonMap[SDL_CONTROLLER_BUTTON_X] = VANILLA_BTN_X;
        g_defaultButtonMap[SDL_CONTROLLER_BUTTON_Y] = VANILLA_BTN_Y;
        g_defaultButtonMap[SDL_CONTROLLER_BUTTON_BACK] = VANILLA_BTN_MINUS;
        g_defaultButtonMap[SDL_CONTROLLER_BUTTON_GUIDE] = VANILLA_BTN_HOME;
        g_defaultButtonMap[SDL_CONTROLLER_BUTTON_MISC1] = VANILLA_BTN_TV;
        g_defaultButtonMap[SDL_CONTROLLER_BUTTON_START] = VANILLA_BTN_PLUS;
        g_defaultButtonMap[SDL_CONTROLLER_BUTTON_LEFTSTICK] = VANILLA_BTN_L3;
        g_defaultButtonMap[SDL_CONTROLLER_BUTTON_RIGHTSTICK] = VANILLA_BTN_R3;
        g_defaultButtonMap[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] = VANILLA_BTN_L;
        g_defaultButtonMap[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = VANILLA_BTN_R;
        g_defaultButtonMap[SDL_CONTROLLER_BUTTON_DPAD_UP] = VANILLA_BTN_UP;
        g_defaultButtonMap[SDL_CONTROLLER_BUTTON_DPAD_DOWN] = VANILLA_BTN_DOWN;
        g_defaultButtonMap[SDL_CONTROLLER_BUTTON_DPAD_LEFT] = VANILLA_BTN_LEFT;
        g_defaultButtonMap[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] = VANILLA_BTN_RIGHT;
        g_defaultAxisMap[SDL_CONTROLLER_AXIS_LEFTX] = VANILLA_AXIS_L_X;
        g_defaultAxisMap[SDL_CONTROLLER_AXIS_LEFTY] = VANILLA_AXIS_L_Y;
        g_defaultAxisMap[SDL_CONTROLLER_AXIS_RIGHTX] = VANILLA_AXIS_R_X;
        g_defaultAxisMap[SDL_CONTROLLER_AXIS_RIGHTY] = VANILLA_AXIS_R_Y;
        g_defaultAxisMap[SDL_CONTROLLER_AXIS_TRIGGERLEFT] = VANILLA_BTN_ZL;
        g_defaultAxisMap[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] = VANILLA_BTN_ZR;
        g_defaultMapsInitialized = true;
    }
}

void GamepadHandler::setKeyMap(KeyMap *keyMap)
{
    m_keyMap = keyMap;
}

void GamepadHandler::signalNextButtonOrAxis(bool e)
{
    m_signalNext = e;
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

void GamepadHandler::clear()
{
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++) {
        m_buttonMap[i] = -1;
    }
    for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++) {
        m_axisMap[i] = -1;
    }
}

void GamepadHandler::setButton(SDL_GameControllerButton sdl_btn, int vanilla_btn)
{
    m_mapMutex.lock();
    m_buttonMap[sdl_btn] = vanilla_btn;
    m_mapMutex.unlock();
}

void GamepadHandler::setAxis(SDL_GameControllerAxis sdl_axis, int vanilla_axis)
{
    m_mapMutex.lock();
    m_axisMap[sdl_axis] = vanilla_axis;
    m_mapMutex.unlock();
}

void GamepadHandler::save()
{
    saveCustomProfile();
}

QString GamepadHandler::getConfigFilename(SDL_JoystickGUID guid)
{
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation));
    dir = dir.filePath("vanilla");
    dir.mkpath(".");

    char buf[33];
    SDL_JoystickGetGUIDString(guid, buf, sizeof(buf));

    return dir.filePath(QStringLiteral("controller-%1.xml").arg(buf));
}

static int g_serializationVersion = 1;

void GamepadHandler::saveCustomProfile()
{
    m_mutex.lock();
    if (m_controller) {
        SDL_JoystickGUID guid = SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(m_controller));
        QFile file(getConfigFilename(guid));
        if (file.open(QFile::WriteOnly)) {
            QXmlStreamWriter writer(&file);
            writer.setAutoFormatting(true);

            writer.writeStartDocument();

            writer.writeStartElement(QStringLiteral("VanillaControllerMap"));

            writer.writeAttribute(QStringLiteral("Version"), QString::number(g_serializationVersion));

            writer.writeStartElement(QStringLiteral("Buttons"));
            for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++) {
                if (m_buttonMap[i] != g_defaultButtonMap[i]) {
                    writer.writeStartElement(QStringLiteral("Button"));
                    writer.writeAttribute(QStringLiteral("Id"), QString::number(i));
                    writer.writeCharacters(QString::number(m_buttonMap[i]));
                    writer.writeEndElement(); // Button
                }
            }
            writer.writeEndElement(); // Buttons

            writer.writeStartElement(QStringLiteral("Axes"));
            for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++) {
                if (m_axisMap[i] != g_defaultAxisMap[i]) {
                    writer.writeStartElement(QStringLiteral("Axis"));
                    writer.writeAttribute(QStringLiteral("Id"), QString::number(i));
                    writer.writeCharacters(QString::number(m_axisMap[i]));
                    writer.writeEndElement(); // Axis
                }
            }
            writer.writeEndElement(); // Axes

            writer.writeEndElement(); // VanillaControllerMap

            writer.writeEndDocument();

            file.close();
        }
    }
    m_mutex.unlock();
}

void enableSensorIfAvailable(SDL_GameController *controller, SDL_SensorType sensor)
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
                enableSensorIfAvailable(m_controller, SDL_SENSOR_ACCEL);
                enableSensorIfAvailable(m_controller, SDL_SENSOR_GYRO);

                SDL_JoystickGUID guid = SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(m_controller));
                QFile f(getConfigFilename(guid));
                clear();
                bool loaded = false;
                if (f.open(QFile::ReadOnly)) {
                    // TODO: Load custom profile if available
                    
                    f.close();
                }

                if (!loaded) {
                    // Set defaults
                    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++) {
                        m_buttonMap[i] = g_defaultButtonMap[i];
                    }
                    for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++) {
                        m_axisMap[i] = g_defaultAxisMap[i];
                    }
                }
            } else {
                m_controller = nullptr;
            }
            m_nextGamepad = -2;
        }
    
        if (m_controller) {
            uint16_t amount = m_vibrate ? 0xFFFF : 0;
            SDL_GameControllerRumble(m_controller, amount, amount, 0);
        }

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
                    int vanilla_btn = m_buttonMap[event.cbutton.button];
                    if (vanilla_btn != -1) {
                        emit buttonStateChanged(vanilla_btn, event.type == SDL_CONTROLLERBUTTONDOWN ? INT16_MAX : 0);
                    }
                    if (m_signalNext && event.type == SDL_CONTROLLERBUTTONDOWN) {
                        emit buttonPressed(static_cast<SDL_GameControllerButton>(event.cbutton.button));
                    }
                }
                break;
            case SDL_CONTROLLERAXISMOTION:
                if (m_controller && event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(m_controller))) {
                    static Sint16 cached_axes[SDL_CONTROLLER_AXIS_MAX] = {0};

                    if (event.caxis.value != cached_axes[event.caxis.axis]) {
                        int vanilla_axis = m_axisMap[event.caxis.axis];
                        Sint16 axis_value = event.caxis.value;
                        if (vanilla_axis != -1) {
                            emit buttonStateChanged(vanilla_axis, axis_value);
                        }
                        if (m_signalNext) {
                            static const int threshold = 1024;
                            if (abs(event.caxis.value - cached_axes[event.caxis.axis]) > threshold) {
                                emit axisMoved(static_cast<SDL_GameControllerAxis>(event.caxis.axis));
                            }
                        }
                        cached_axes[event.caxis.axis] = event.caxis.value;
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

        m_mutex.unlock();

        // Don't spam CPU cycles, still allow for up to 200Hz polling (for reference, Wii U gamepad is 180Hz)
        SDL_Delay(5);

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
    if (!m_controller && m_keyMap) {
        auto it = m_keyMap->find(key);
        if (it != m_keyMap->end()) {
            emit buttonStateChanged(it->second, INT16_MAX);
        }
    }
}

void GamepadHandler::keyReleased(Qt::Key key)
{
    if (!m_controller && m_keyMap) {
        auto it = m_keyMap->find(key);
        if (it != m_keyMap->end()) {
            emit buttonStateChanged(it->second, 0);
        }
    }
}

int GamepadHandler::button(SDL_GameControllerButton sdl_btn)
{
    int b;
    m_mapMutex.lock();
    b = m_buttonMap[sdl_btn];
    m_mapMutex.unlock();
    return b;
}

int GamepadHandler::axis(SDL_GameControllerAxis sdl_axis)
{
    int b;
    m_mapMutex.lock();
    b = m_axisMap[sdl_axis];
    m_mapMutex.unlock();
    return b;
}