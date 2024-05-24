#ifndef INPUT_CONFIG_DIALOG_H
#define INPUT_CONFIG_DIALOG_H

#include <QDialog>
#include <QPushButton>
#include <QTimer>

class InputConfigButton : public QWidget
{
    Q_OBJECT
public:
    InputConfigButton(const QString &name, QWidget *parent = nullptr);

    Qt::Key key() const { return m_key; }

public slots:
    void setKey(Qt::Key key);

protected:
    virtual bool eventFilter(QObject *object, QEvent *event) override;

private:
    QPushButton *m_button;
    Qt::Key m_key;
    QTimer *m_timer;
    int m_timerState;

private slots:
    void buttonClicked(bool e);

    void timerTimeout();

};

class InputConfigDialog : public QDialog
{
    Q_OBJECT
public:
    InputConfigDialog(QWidget *parent = nullptr);

public slots:
    virtual void accept() override;

private:
    InputConfigButton *createButton(const QString &name, int vanillaBtn);

    std::map<int, InputConfigButton *> m_buttons;
};

#endif // INPUT_CONFIG_DIALOG_H