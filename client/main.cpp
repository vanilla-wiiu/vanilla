#include <QApplication>
#include <QMainWindow>

#include "mainwindow.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    MainWindow mw;
    mw.show();

    return app.exec();
}