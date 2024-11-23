#include "inputconfigdialog.h"

#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QGridLayout>
#include <QLabel>

#include <vanilla.h>

InputConfigDialog::InputConfigDialog(QWidget *parent) : QDialog(parent)
{
}

InputConfigDialog::InputConfigDialog(KeyMap *keyMap, QWidget *parent) : InputConfigDialog(parent)
{
    m_keyMap = keyMap;
    m_gamepadHandler = nullptr;

    createLayout<InputConfigKeyButton>();

    for (auto it = m_keyMap->cbegin(); it != m_keyMap->cend(); it++) {
        InputConfigKeyButton *btn = static_cast<InputConfigKeyButton *>(m_buttons[it->second]);
        if (btn) {
            btn->setKey(it->first);
        }
    }

    connectSoloSignals();
}

InputConfigDialog::InputConfigDialog(GamepadHandler *gamepadHandler, QWidget *parent) : InputConfigDialog(parent)
{
    m_keyMap = nullptr;
    m_gamepadHandler = gamepadHandler;

    m_gamepadHandler->signalNextButtonOrAxis(true);

    connect(m_gamepadHandler, &GamepadHandler::axisMoved, this, &InputConfigDialog::setControllerAxis);
    connect(m_gamepadHandler, &GamepadHandler::buttonPressed, this, &InputConfigDialog::setControllerButton);

    createLayout<InputConfigControllerButton>();

    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++) {
        SDL_GameControllerButton b = static_cast<SDL_GameControllerButton>(i);
        int id = m_gamepadHandler->button(b);
        if (id != -1) {
            if (InputConfigControllerButton *btn = static_cast<InputConfigControllerButton *>(m_buttons[id])) {
                btn->setButton(b);
            }
        }
    }

    for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++) {
        SDL_GameControllerAxis a = static_cast<SDL_GameControllerAxis>(i);
        int id = m_gamepadHandler->axis(a);
        if (id != -1) {
            if (InputConfigControllerButton *btn = static_cast<InputConfigControllerButton *>(m_buttons[id])) {
                btn->setAxis(a);
            }
        }
    }

    connectSoloSignals();
}

InputConfigDialog::~InputConfigDialog()
{
    if (m_gamepadHandler) {
        m_gamepadHandler->signalNextButtonOrAxis(false);
    }
}

template<typename T>
void InputConfigDialog::createLayout()
{
    QGridLayout *layout = new QGridLayout(this);

    layout->addWidget(createButton<T>(tr("L"), VANILLA_BTN_L), 2, 1);
    layout->addWidget(createButton<T>(tr("ZL"), VANILLA_BTN_ZL), 3, 1);

    layout->addWidget(createButton<T>(tr("R"), VANILLA_BTN_R), 2, 3);
    layout->addWidget(createButton<T>(tr("ZR"), VANILLA_BTN_ZR), 3, 3);

    layout->addWidget(createButton<T>(tr("Left Stick Up"), VANILLA_AXIS_L_UP), 4, 0);
    layout->addWidget(createButton<T>(tr("Left Stick Down"), VANILLA_AXIS_L_DOWN), 5, 0);
    layout->addWidget(createButton<T>(tr("Left Stick Left"), VANILLA_AXIS_L_LEFT), 6, 0);
    layout->addWidget(createButton<T>(tr("Left Stick Right"), VANILLA_AXIS_L_RIGHT), 7, 0);
    layout->addWidget(createButton<T>(tr("Left Stick Click"), VANILLA_BTN_L3), 8, 0);

    layout->addWidget(createButton<T>(tr("D-Pad Up"), VANILLA_BTN_UP), 10, 0);
    layout->addWidget(createButton<T>(tr("D-Pad Down"), VANILLA_BTN_DOWN), 11, 0);
    layout->addWidget(createButton<T>(tr("D-Pad Left"), VANILLA_BTN_LEFT), 12, 0);
    layout->addWidget(createButton<T>(tr("D-Pad Right"), VANILLA_BTN_RIGHT), 13, 0);

    layout->addWidget(createButton<T>(tr("TV"), VANILLA_BTN_TV), 14, 1, 1, 1);
    layout->addWidget(createButton<T>(tr("Home"), VANILLA_BTN_HOME), 14, 2);

    layout->addWidget(createButton<T>(tr("Right Stick Up"), VANILLA_AXIS_R_UP), 2, 4);
    layout->addWidget(createButton<T>(tr("Right Stick Down"), VANILLA_AXIS_R_DOWN), 3, 4);
    layout->addWidget(createButton<T>(tr("Right Stick Left"), VANILLA_AXIS_R_LEFT), 4, 4);
    layout->addWidget(createButton<T>(tr("Right Stick Right"), VANILLA_AXIS_R_RIGHT), 5, 4);
    layout->addWidget(createButton<T>(tr("Right Stick Click"), VANILLA_BTN_R3), 6, 4);

    layout->addWidget(createButton<T>(tr("A"), VANILLA_BTN_A), 8, 4);
    layout->addWidget(createButton<T>(tr("B"), VANILLA_BTN_B), 9, 4);
    layout->addWidget(createButton<T>(tr("X"), VANILLA_BTN_X), 10, 4);
    layout->addWidget(createButton<T>(tr("Y"), VANILLA_BTN_Y), 11, 4);

    layout->addWidget(createButton<T>(tr("Start/Plus"), VANILLA_BTN_PLUS), 13, 4);
    layout->addWidget(createButton<T>(tr("Select/Minus"), VANILLA_BTN_MINUS), 14, 4);

    QLabel *imageLbl = new QLabel(this);
    QPixmap imgPix;
    imgPix.load(QStringLiteral(":/gamepad-diagram.svg"));
    imageLbl->setPixmap(imgPix);
    layout->addWidget(imageLbl, 4, 1, 10, 3);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setCenterButtons(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons, 16, 0, 1, 5);
}

template<typename T>
InputConfigButtonBase *InputConfigDialog::createButton(const QString &name, int vanillaBtn)
{
    InputConfigButtonBase *i = new T(name, this);
    m_buttons[vanillaBtn] = i;
    return i;
}

void InputConfigDialog::accept()
{
    if (m_keyMap) {
        m_keyMap->clear();

        for (auto it = m_buttons.cbegin(); it != m_buttons.cend(); it++) {
            InputConfigKeyButton *keyBtn = static_cast<InputConfigKeyButton *>(it->second);
            Qt::Key key = keyBtn->key();
            if (key != Qt::Key_unknown) {
                (*m_keyMap)[key] = it->first;
            }
        }

        m_keyMap->save(KeyMap::getConfigFilename());
    } else if (m_gamepadHandler) {
        m_gamepadHandler->clear();

        for (auto it = m_buttons.cbegin(); it != m_buttons.cend(); it++) {
            InputConfigControllerButton *cBtn = static_cast<InputConfigControllerButton *>(it->second);
            if (cBtn->button() != SDL_CONTROLLER_BUTTON_INVALID) {
                m_gamepadHandler->setButton(cBtn->button(), it->first);
            } else if (cBtn->axis() != SDL_CONTROLLER_AXIS_INVALID) {
                m_gamepadHandler->setAxis(cBtn->axis(), it->first);
            }
        }

        m_gamepadHandler->save();
    }

    QDialog::accept();
}

InputConfigButtonBase::InputConfigButtonBase(const QString &name, QWidget *parent) : QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);

    layout->addStretch();

    QLabel *label = new QLabel(name, this);
    layout->addWidget(label);

    m_button = new QPushButton(this);
    m_button->setCheckable(true);
    connect(m_button, &QPushButton::clicked, this, &InputConfigButtonBase::buttonClicked);
    connect(m_button, &QPushButton::toggled, this, &InputConfigButtonBase::toggled);
    connect(m_button, &QPushButton::clicked, this, &InputConfigButtonBase::checked);
    layout->addWidget(m_button);

    layout->addStretch();

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &InputConfigButtonBase::timerTimeout);
}

InputConfigKeyButton::InputConfigKeyButton(const QString &name, QWidget *parent) : InputConfigButtonBase(name, parent)
{
    button()->installEventFilter(this);
    m_key = Qt::Key_unknown;
    buttonClicked(false);
}

void InputConfigKeyButton::setKey(Qt::Key key)
{
    m_key = key;
    buttonClicked(false);
}

void InputConfigButtonBase::buttonClicked(bool e)
{
    if (e) {
        m_timerState = 6;
        m_timer->start();
        timerTimeout();
    } else {
        m_timer->stop();
        m_button->setText(text());
        m_button->setChecked(false);
    }
}

void InputConfigButtonBase::setChecked(bool e)
{
    m_button->setChecked(e);
    buttonClicked(e);
}

QString InputConfigKeyButton::text() const
{
    return m_key == Qt::Key_unknown ? tr("<none>") : QKeySequence(m_key).toString();
}

bool InputConfigKeyButton::eventFilter(QObject *object, QEvent *event)
{
    if (object == button() && button()->isChecked() && event->type() == QEvent::KeyPress) {
        QKeyEvent *key = dynamic_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape) {
            // Don't set
            buttonClicked(false);
        } else if (key->key() == Qt::Key_Backspace) {
            setKey(Qt::Key_unknown);
        } else {
            // Set whatever was pressed
            setKey(static_cast<Qt::Key>(key->key()));
        }
        return true;
    }
    return false;
}

void InputConfigButtonBase::timerTimeout()
{
    m_timerState--;
    if (m_timerState == 0) {
        buttonClicked(false);
    } else {
        m_button->setText(tr("Waiting for input (%1)\n<Backspace> to clear").arg(m_timerState));
    }
}

InputConfigControllerButton::InputConfigControllerButton(const QString &name, QWidget *parent) : InputConfigButtonBase(name, parent)
{
    m_button = SDL_CONTROLLER_BUTTON_INVALID;
    m_axis = SDL_CONTROLLER_AXIS_INVALID;
    buttonClicked(false);
}

QString InputConfigControllerButton::text() const
{
    if (m_button != SDL_CONTROLLER_BUTTON_INVALID) {
        return SDL_GameControllerGetStringForButton(m_button);
    } else if (m_axis != SDL_CONTROLLER_AXIS_INVALID) {
        return SDL_GameControllerGetStringForAxis(m_axis);
    }
    return tr("<none>");
}

void InputConfigControllerButton::setButton(SDL_GameControllerButton button)
{
    m_button = button;
    m_axis = SDL_CONTROLLER_AXIS_INVALID;
    buttonClicked(false);
}

void InputConfigControllerButton::setAxis(SDL_GameControllerAxis axis)
{
    m_button = SDL_CONTROLLER_BUTTON_INVALID;
    m_axis = axis;
    buttonClicked(false);
}

InputConfigButtonBase *InputConfigDialog::findCheckedButton()
{
    for (auto it = m_buttons.cbegin(); it != m_buttons.cend(); it++) {
        InputConfigButtonBase *test = it->second;
        if (test && test->isChecked()) {
            return test;
        }
    }
    return nullptr;
}

void InputConfigDialog::setControllerAxis(SDL_GameControllerAxis axis)
{
    if (InputConfigButtonBase *b = findCheckedButton()) {
        InputConfigControllerButton *c = static_cast<InputConfigControllerButton *>(b);
        c->setAxis(axis);
    }
}

void InputConfigDialog::setControllerButton(SDL_GameControllerButton button)
{
    if (InputConfigButtonBase *b = findCheckedButton()) {
        InputConfigControllerButton *c = static_cast<InputConfigControllerButton *>(b);
        c->setButton(button);
    }
}

void InputConfigDialog::connectSoloSignals()
{
    for (auto it = m_buttons.cbegin(); it != m_buttons.cend(); it++) {
        InputConfigButtonBase *btn = it->second;
        if (btn)
            connect(btn, &InputConfigButtonBase::checked, this, &InputConfigDialog::buttonChecked);
    }
}

void InputConfigDialog::uncheckAllExcept(InputConfigButtonBase *except)
{
    for (auto it = m_buttons.cbegin(); it != m_buttons.cend(); it++) {
        InputConfigButtonBase *b = it->second;
        if (b && b != except) {
            b->setChecked(false);
        }
    }
}

void InputConfigDialog::buttonChecked(bool e)
{
    if (e) {
        uncheckAllExcept(static_cast<InputConfigButtonBase *>(sender()));
    }
}
