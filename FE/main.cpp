/**
 * @file main.cpp
 * @brief Program entry point. Initializes Qt, GStreamer, SSL, and opens the main window.
 */
#include "mainwindow.h"
#include "logindialog.h"
#include "applicationinitializer.h"
#include "configmanager.h"

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    qDebug() << "[main] Qt application created";

    // Initialize all system components
    if (!ApplicationInitializer::initializeEnvironment()) {
        qWarning() << "[main] Failed to initialize some system components";
    }

    // Load configuration
    ConfigManager::instance().loadDefaults();

    // Show login dialog
    LoginDialog loginDialog;
    if (loginDialog.exec() != QDialog::Accepted) {
        qDebug() << "[main] Login canceled. Exiting application.";
        return 0;
    }

    // Create and show main window
    qDebug() << "[main] Opening main window...";
    MainWindow w;
    w.show();

    qDebug() << "[main] Main window shown";
    qDebug() << "=================================";

    return a.exec();
}
