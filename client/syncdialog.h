#ifndef SYNC_DIALOG_H
#define SYNC_DIALOG_H

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>

class SyncDialog : public QDialog
{
    Q_OBJECT
public:
    SyncDialog(QWidget *parent = nullptr);

    void setup(const QString &wirelessInterface);

private:
    void updateLabels();
    QPushButton *createButton(const QString &text, int id);
    void launchSync();

    static const int g_symbolCount = 4;
    static QString g_symbols[g_symbolCount];

    QLabel *m_labels[g_symbolCount];
    int8_t m_code[g_symbolCount];
    QHBoxLayout *m_buttonLayout;
    QString m_wirelessInterface;

private slots:
    void buttonClicked();

};

#endif // SYNC_DIALOG_H