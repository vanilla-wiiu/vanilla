#include "backendinitdialog.h"

#include <QLabel>
#include <QVBoxLayout>

BackendInitDialog::BackendInitDialog(QWidget *parent) : QDialog(parent)
{
    QVBoxLayout *l = new QVBoxLayout(this);

    l->addWidget(new QLabel(tr("Initializing backend...")));
}
