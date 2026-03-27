#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName("GPXPlanner");
    app.setApplicationName("GPXPlanner");
    app.setApplicationVersion("1.0");

    MainWindow w;
    w.show();
    return app.exec();
}
