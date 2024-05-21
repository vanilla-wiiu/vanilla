#include "inputconfigdialog.h"

#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QGridLayout>
#include <QLabel>

#include <vanilla.h>

#include "keymap.h"

InputConfigDialog::InputConfigDialog(QWidget *parent) : QDialog(parent)
{
    QGridLayout *layout = new QGridLayout(this);

    layout->addWidget(createButton(tr("L"), VANILLA_BTN_L), 0, 1);
    layout->addWidget(createButton(tr("ZL"), VANILLA_BTN_ZL), 1, 1);

    layout->addWidget(createButton(tr("R"), VANILLA_BTN_R), 0, 3);
    layout->addWidget(createButton(tr("ZR"), VANILLA_BTN_ZR), 1, 3);

    layout->addWidget(createButton(tr("Left Stick Up"), VANILLA_AXIS_L_UP), 3, 0);
    layout->addWidget(createButton(tr("Left Stick Down"), VANILLA_AXIS_L_DOWN), 4, 0);
    layout->addWidget(createButton(tr("Left Stick Left"), VANILLA_AXIS_L_LEFT), 5, 0);
    layout->addWidget(createButton(tr("Left Stick Right"), VANILLA_AXIS_L_RIGHT), 6, 0);

    layout->addWidget(createButton(tr("D-Pad Up"), VANILLA_BTN_UP), 9, 0);
    layout->addWidget(createButton(tr("D-Pad Down"), VANILLA_BTN_DOWN), 10, 0);
    layout->addWidget(createButton(tr("D-Pad Left"), VANILLA_BTN_LEFT), 11, 0);
    layout->addWidget(createButton(tr("D-Pad Right"), VANILLA_BTN_RIGHT), 12, 0);

    layout->addWidget(createButton(tr("Home"), VANILLA_BTN_HOME), 14, 1, 1, 3);

    layout->addWidget(createButton(tr("Right Stick Up"), VANILLA_AXIS_R_UP), 2, 4);
    layout->addWidget(createButton(tr("Right Stick Down"), VANILLA_AXIS_R_DOWN), 3, 4);
    layout->addWidget(createButton(tr("Right Stick Left"), VANILLA_AXIS_R_LEFT), 4, 4);
    layout->addWidget(createButton(tr("Right Stick Right"), VANILLA_AXIS_R_RIGHT), 5, 4);

    layout->addWidget(createButton(tr("A"), VANILLA_BTN_A), 7, 4);
    layout->addWidget(createButton(tr("B"), VANILLA_BTN_B), 8, 4);
    layout->addWidget(createButton(tr("X"), VANILLA_BTN_X), 9, 4);
    layout->addWidget(createButton(tr("Y"), VANILLA_BTN_Y), 10, 4);

    layout->addWidget(createButton(tr("Start/Plus"), VANILLA_BTN_PLUS), 12, 4);
    layout->addWidget(createButton(tr("Select/Minus"), VANILLA_BTN_MINUS), 13, 4);

    QLabel *imageLbl = new QLabel(this);
    QPixmap imgPix;
    imgPix.load(QStringLiteral(":/com.mattkc.vanilla.svg"));
    imageLbl->setPixmap(imgPix);
    layout->addWidget(imageLbl, 2, 1, 12, 3);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setCenterButtons(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons, 15, 0, 1, 5);

    for (auto it = KeyMap::instance.cbegin(); it != KeyMap::instance.cend(); it++) {
        InputConfigButton *btn = m_buttons[it->second];
        if (btn) {
            btn->setKey(it->first);
        }
    }

    setWindowTitle(tr("Keyboard Configuration"));
}

InputConfigButton *InputConfigDialog::createButton(const QString &name, int vanillaBtn)
{
    InputConfigButton *i = new InputConfigButton(name, this);
    m_buttons[vanillaBtn] = i;
    return i;
}

void InputConfigDialog::accept()
{
    KeyMap::instance.clear();

    for (auto it = m_buttons.cbegin(); it != m_buttons.cend(); it++) {
        Qt::Key key = it->second->key();
        if (key != Qt::Key_unknown) {
            KeyMap::instance[key] = it->first;
        }
    }

    KeyMap::instance.save(KeyMap::getConfigFilename());

    QDialog::accept();
}

InputConfigButton::InputConfigButton(const QString &name, QWidget *parent) : QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);

    layout->addStretch();

    QLabel *label = new QLabel(name, this);
    layout->addWidget(label);

    m_button = new QPushButton(this);
    m_button->setCheckable(true);
    connect(m_button, &QPushButton::clicked, this, &InputConfigButton::buttonClicked);
    m_button->installEventFilter(this);
    layout->addWidget(m_button);

    layout->addStretch();

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &InputConfigButton::timerTimeout);

    m_key = Qt::Key_unknown;
    buttonClicked(false);
}

void InputConfigButton::setKey(Qt::Key key)
{
    m_key = key;
    buttonClicked(false);
}

void InputConfigButton::buttonClicked(bool e)
{
    if (e) {
        m_timerState = 6;
        m_timer->start();
        timerTimeout();
    } else {
        m_timer->stop();
        m_button->setText(m_key == Qt::Key_unknown ? tr("<none>") : QKeySequence(m_key).toString());
        m_button->setChecked(false);
    }
}

bool InputConfigButton::eventFilter(QObject *object, QEvent *event)
{
    if (object == m_button && m_button->isChecked() && event->type() == QEvent::KeyPress) {
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

void InputConfigButton::timerTimeout()
{
    m_timerState--;
    if (m_timerState == 0) {
        buttonClicked(false);
    } else {
        m_button->setText(tr("Waiting for input (%1)\n<Backspace> to clear").arg(m_timerState));
    }
}