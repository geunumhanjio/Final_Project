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

int main(int argc, char *argv[])
{
    qDebug() << "=== VEDA CCTV System Starting ===";

#ifdef GST_BIN_PATH
    QString gstBinPath = QString::fromUtf8(GST_BIN_PATH);
    gstBinPath = QDir::toNativeSeparators(gstBinPath);
    QByteArray currentPath = qgetenv("PATH");
    QByteArray newPath = gstBinPath.toLocal8Bit() + ";" + currentPath;
    qputenv("PATH", newPath);

    qDebug() << "[main] GStreamer bin path:" << gstBinPath;

    QDir binDir(gstBinPath);
    binDir.cdUp();
    QString libPath = binDir.absolutePath() + "\\lib\\gstreamer-1.0";
    qputenv("GST_PLUGIN_PATH", libPath.toLocal8Bit());
    qDebug() << "[main] GST_PLUGIN_PATH set to:" << libPath;
#else
    qWarning() << "[main] GST_BIN_PATH not defined in CMakeLists.txt!";
#endif

    qputenv("GST_DEBUG", "1");
    qDebug() << "[main] GST_DEBUG level set to: 1";

void applyNativeWindowIcon(QWidget *widget)
{
    if (!widget) {
        return;
    }

    // GStreamer 초기화 전에 환경 확인
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

    qDebug() << "[main] Initializing GStreamer...";
    gst_init(&argc, &argv);
    gst_quality_monitor_register(NULL);
    qDebug() << "[main] GStreamer Initialized.";

    setupSslContext();

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
