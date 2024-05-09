#include <QApplication>
#include <QMainWindow>

#include "vanilla.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    Vanilla mw;
    mw.show();

    return app.exec();
}