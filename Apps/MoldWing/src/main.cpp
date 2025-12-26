/*
 *  MoldWing - Oblique Photography 3D Model Editor
 *  Main entry point with logging and crash handling
 */

#include <QApplication>
#include <QMessageBox>
#include "MainWindow.hpp"
#include "Core/Logger.hpp"

#ifdef _WIN32
#include <Windows.h>
#endif

int main(int argc, char* argv[])
{
#ifdef _WIN32
    // 设置控制台代码页为 UTF-8，解决中文乱码问题
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    // 初始化日志系统 (在 QApplication 之前)
    if (!MoldWing::Logger::instance().initialize("MoldWing", true)) {
        // 如果日志初始化失败，仍然继续运行，只是没有日志
        // 可以考虑在这里显示一个警告
    }

    LOG_INFO("应用程序启动中...");
    LOG_DEBUG("命令行参数数量: {}", argc);

    QApplication app(argc, argv);

    // Set application info
    QApplication::setApplicationName("MoldWing");
    QApplication::setApplicationVersion("0.1-dev");
    QApplication::setOrganizationName("MoldWing");

    LOG_INFO("Qt 应用程序已初始化");

    // Create and show main window
    MoldWing::MainWindow mainWindow;
    mainWindow.show();

    LOG_INFO("主窗口已显示");

    int result = app.exec();

    LOG_INFO("应用程序退出，返回码: {}", result);

    // 关闭日志系统
    MoldWing::Logger::instance().shutdown();

    return result;
}
