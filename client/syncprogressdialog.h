#ifndef SYNC_PROGRESS_H
#define SYNC_PROGRESS_H

#include <QDialog>

class SyncProgressDialog : public QDialog
{
public:
    SyncProgressDialog(QWidget *parent = nullptr);

    void start(const QString &wirelessInterface, uint16_t code);

private slots:
    void serverReturned();

};

#endif // SYNC_PROGRESS_H