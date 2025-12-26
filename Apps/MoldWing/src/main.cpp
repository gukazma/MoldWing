/*
 *  MoldWing - Oblique Photography 3D Model Editor
 *  S1.1: Minimal Qt window verification
 */

#include <QApplication>
#include "MainWindow.hpp"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // Set application info
    QApplication::setApplicationName("MoldWing");
    QApplication::setApplicationVersion("0.1-dev");
    QApplication::setOrganizationName("MoldWing");

    // Create and show main window
    MoldWing::MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
