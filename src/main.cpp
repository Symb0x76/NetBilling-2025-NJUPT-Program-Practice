#include <QApplication>

#include "ElaApplication.h"
#include "ui/mainwindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    eApp->init();

    MainWindow w;
    w.moveToCenter();
    w.show();

    return app.exec();
}
