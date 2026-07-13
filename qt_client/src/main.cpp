#include "main_window.h"

#include <QApplication>
#include <QFont>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("DeviceOps");
    QApplication::setOrganizationName("DeviceOps");

    QFont font = QApplication::font();
    font.setPointSize(10);
    QApplication::setFont(font);

    MainWindow window;
    window.resize(1440, 900);
    window.show();

    return app.exec();
}
