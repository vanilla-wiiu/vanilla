#ifndef BACKEND_INIT_DIALOG_H
#define BACKEND_INIT_DIALOG_H

#include <QDialog>

class BackendInitDialog : public QDialog
{
    Q_OBJECT
public:
    BackendInitDialog(QWidget *parent = nullptr);
};

#endif // BACKEND_INIT_DIALOG_H