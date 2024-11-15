#ifndef INPUT_CONFIG_DIALOG_H
#define INPUT_CONFIG_DIALOG_H

#include <SDL2/SDL.h>
#include <QDialog>
#include <QPushButton>
#include <QTimer>

#include "gamepadhandler.h"
#include "keymap.h"

class InputConfigButtonBase : public QWidget
{
    Q_OBJECT
public:
    InputConfigButtonBase(const QString &name, QWidget *parent = nullptr);

    bool isChecked() const { return m_button->isChecked(); }
    void setChecked(bool e);

private:
    QPushButton *m_button;
    QTimer *m_timer;
    int m_timerState;

protected:
    QPushButton *button() { return m_button; }
    virtual QString text() const = 0;

signals:
    void checked(bool e);
    void toggled(bool e);

protected slots:
    void buttonClicked(bool e);

private slots:
    void timerTimeout();

};

class InputConfigKeyButton : public InputConfigButtonBase
{
    Q_OBJECT
public:
    InputConfigKeyButton(const QString &name, QWidget *parent = nullptr);

    Qt::Key key() const { return m_key; }

    virtual QString text() const override;

public slots:
    void setKey(Qt::Key key);

protected:
    virtual bool eventFilter(QObject *object, QEvent *event) override;

private:
    Qt::Key m_key;

};

class InputConfigControllerButton : public InputConfigButtonBase
{
    Q_OBJECT
public:
    InputConfigControllerButton(const QString &name, QWidget *parent = nullptr);

    SDL_GameControllerButton button() const { return m_button; }
    SDL_GameControllerAxis axis() const { return m_axis; }

    virtual QString text() const override;

public slots:
    void setButton(SDL_GameControllerButton btn);
    void setAxis(SDL_GameControllerAxis axis);

private:
    SDL_GameControllerButton m_button;
    SDL_GameControllerAxis m_axis;

};

class InputConfigDialog : public QDialog
{
    Q_OBJECT
public:
    InputConfigDialog(KeyMap *keyMap, QWidget *parent = nullptr);
    InputConfigDialog(GamepadHandler *gamepadHandler, QWidget *parent = nullptr);
    virtual ~InputConfigDialog() override;

public slots:
    virtual void accept() override;

private:
    InputConfigDialog(QWidget *parent = nullptr);

    template<typename T>
    void createLayout();

    template<typename T>
    InputConfigButtonBase *createButton(const QString &name, int vanillaBtn);

    InputConfigButtonBase *findCheckedButton();

    void connectSoloSignals();
    void uncheckAllExcept(InputConfigButtonBase *except);

    std::map<int, InputConfigButtonBase *> m_buttons;

    KeyMap *m_keyMap;
    GamepadHandler *m_gamepadHandler;

private slots:
    void setControllerAxis(SDL_GameControllerAxis axis);
    void setControllerButton(SDL_GameControllerButton button);
    void buttonChecked(bool e);
};

#endif // INPUT_CONFIG_DIALOG_H