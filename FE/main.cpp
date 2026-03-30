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
#include <QIcon>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef Q_OS_WIN
namespace {

void applyNativeWindowIcon(QWidget *widget)
{
    if (!widget) {
        return;
    }

    const HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    if (!hwnd) {
        return;
    }

    static HICON s_smallIcon = nullptr;
    static HICON s_bigIcon = nullptr;

    if (!s_smallIcon) {
        s_smallIcon = static_cast<HICON>(LoadImageW(
            GetModuleHandleW(nullptr),
            L"IDI_APP_ICON",
            IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON),
            GetSystemMetrics(SM_CYSMICON),
            0));
    }

    if (!s_bigIcon) {
        s_bigIcon = static_cast<HICON>(LoadImageW(
            GetModuleHandleW(nullptr),
            L"IDI_APP_ICON",
            IMAGE_ICON,
            GetSystemMetrics(SM_CXICON),
            GetSystemMetrics(SM_CYICON),
            0));
    }

    if (s_smallIcon) {
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(s_smallIcon));
    }
    if (s_bigIcon) {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(s_bigIcon));
    }
}

} // namespace
#endif

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    qDebug() << "[main] Qt application created";

    QIcon appIcon;
    appIcon.addFile(QStringLiteral(":/icons/assets/icons/noobigo_app.png"));
    appIcon.addFile(QStringLiteral(":/icons/assets/icons/noobigo_app.ico"));
    if (!appIcon.isNull()) {
        a.setWindowIcon(appIcon);
    }

    // Initialize all system components
    if (!ApplicationInitializer::initializeEnvironment()) {
        qWarning() << "[main] Failed to initialize some system components";
    }

    // Load configuration
    ConfigManager::instance().loadDefaults();

    // Show login dialog
    LoginDialog loginDialog;
    if (!appIcon.isNull()) {
        loginDialog.setWindowIcon(appIcon);
    }
#ifdef Q_OS_WIN
    QTimer::singleShot(0, &loginDialog, [&loginDialog]() {
        applyNativeWindowIcon(&loginDialog);
    });
#endif
    if (loginDialog.exec() != QDialog::Accepted) {
        qDebug() << "[main] Login canceled. Exiting application.";
        return 0;
    }

    // Create and show main window
    qDebug() << "[main] Opening main window...";
    MainWindow w;
    if (!appIcon.isNull()) {
        w.setWindowIcon(appIcon);
    }
    w.show();
#ifdef Q_OS_WIN
    QTimer::singleShot(0, &w, [&w]() {
        applyNativeWindowIcon(&w);
    });
#endif

    qDebug() << "[main] Main window shown";
    qDebug() << "=================================";

    return a.exec();
}
