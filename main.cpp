#include <QApplication>
#include <QIcon>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Chip8 Emulator");
    QApplication::setOrganizationName("Chip8Emulator");
    QApplication::setApplicationVersion(APP_VERSION); // from VERSION in the .pro file
    QApplication::setWindowIcon(QIcon(":/icons/app.png"));

    MainWindow window;
    window.show();

    return app.exec();
}
