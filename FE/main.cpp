/**
 * @file main.cpp
 * @brief Program entry point. Initializes Qt, GStreamer, SSL, and opens the main window.
 */
#include "mainwindow.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <gst/gst.h>
#include "Video/Gst/GstQualityMonitor.hpp"

/**
 * @brief Register the embedded RTSPS server certificate in the default Qt CA store.
 */
static void setupSslContext()
{
    const QString certResourcePath = ":/crt/env/server.crt";
    qDebug() << "[SSL] Loading certificate from Qt resource:" << certResourcePath;

    QFile certFile(certResourcePath);
    if (!certFile.open(QIODevice::ReadOnly)) {
        qWarning() << "[SSL] Failed to open embedded server.crt from resource.";
        return;
    }

    const QByteArray certData = certFile.readAll();
    certFile.close();

    const QList<QSslCertificate> certs = QSslCertificate::fromData(certData);
    if (certs.isEmpty()) {
        qWarning() << "[SSL] Failed to parse embedded server.crt! RTSPS might fail unless tls-validation-flags=0 is used.";
        return;
    }

    QSslConfiguration config = QSslConfiguration::defaultConfiguration();
    config.addCaCertificates(certs);
    QSslConfiguration::setDefaultConfiguration(config);
    qDebug() << "[SSL] Embedded RTSPS server certificate (server.crt) registered successfully.";
}

int main(int argc, char *argv[])
{
    qDebug() << "=== VEDA CCTV System Starting ===";

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString bundledGstRoot = QDir(appDir).filePath("gstreamer");
    const QString bundledGstBinPath = QDir(bundledGstRoot).filePath("bin");
    const QString bundledRootDll = QDir(appDir).filePath("gstreamer-1.0-0.dll");
    QString gstBinPath;
    QString gstRootPath;

    if (QFileInfo::exists(bundledRootDll) && QDir(bundledGstRoot).exists()) {
        gstBinPath = appDir;
        gstRootPath = bundledGstRoot;
        qDebug() << "[main] Using app-local GStreamer DLLs from:" << gstBinPath;
    } else if (QDir(bundledGstBinPath).exists()) {
        gstBinPath = bundledGstBinPath;
        gstRootPath = bundledGstRoot;
        qDebug() << "[main] Using bundled GStreamer from:" << gstBinPath;
    }

#ifdef GST_BIN_PATH
    if (gstBinPath.isEmpty()) {
        gstBinPath = QString::fromUtf8(GST_BIN_PATH);
        QDir configuredBinDir(gstBinPath);
        configuredBinDir.cdUp();
        gstRootPath = configuredBinDir.absolutePath();
    }
#endif

    if (!gstBinPath.isEmpty()) {
        gstBinPath = QDir::toNativeSeparators(gstBinPath);
        QByteArray currentPath = qgetenv("PATH");
        QByteArray newPath = gstBinPath.toLocal8Bit() + ";" + currentPath;
        qputenv("PATH", newPath);

        qDebug() << "[main] GStreamer bin path:" << gstBinPath;

        if (gstRootPath.isEmpty()) {
            QDir binDir(gstBinPath);
            binDir.cdUp();
            gstRootPath = binDir.absolutePath();
        }

        const QString pluginPath = QDir(gstRootPath).filePath("lib/gstreamer-1.0");
        qputenv("GST_PLUGIN_PATH", QDir::toNativeSeparators(pluginPath).toLocal8Bit());
        qDebug() << "[main] GST_PLUGIN_PATH set to:" << pluginPath;

        const QString scannerPath = QDir(gstRootPath).filePath("libexec/gstreamer-1.0/gst-plugin-scanner.exe");
        if (QFileInfo::exists(scannerPath)) {
            qputenv("GST_PLUGIN_SCANNER", QDir::toNativeSeparators(scannerPath).toLocal8Bit());
            qDebug() << "[main] GST_PLUGIN_SCANNER set to:" << scannerPath;
        }
    } else {
        qWarning() << "[main] No GStreamer runtime path found. Bundled playback/RTSP may fail.";
    }

#ifndef GST_BIN_PATH
    if (gstBinPath.isEmpty()) {
        qWarning() << "[main] GST_BIN_PATH not defined in CMakeLists.txt!";
    }
#endif

    qputenv("GST_DEBUG", "1,libav:0");
    qDebug() << "[main] GST_DEBUG configured to: 1,libav:0";

    QApplication a(argc, argv);
    qDebug() << "[main] Qt application created";

    const QStringList fontResources = {
        QStringLiteral(":/fonts/Pretendard-Regular.otf"),
        QStringLiteral(":/fonts/Pretendard-Medium.otf"),
        QStringLiteral(":/fonts/Pretendard-Bold.otf")
    };

    QString appFontFamily;
    for (const QString &fontPath : fontResources) {
        const int fontId = QFontDatabase::addApplicationFont(fontPath);
        if (fontId < 0) {
            qWarning() << "[Font] Failed to load:" << fontPath;
            continue;
        }

        if (appFontFamily.isEmpty()) {
            appFontFamily = QFontDatabase::applicationFontFamilies(fontId).value(0);
        }
    }

    if (!appFontFamily.isEmpty()) {
        a.setFont(QFont(appFontFamily));
        qDebug() << "[Font] Loaded application font:" << appFontFamily;
    } else {
        qWarning() << "[Font] Pretendard font family could not be loaded.";
    }

    qDebug() << "[main] Initializing GStreamer...";
    gst_init(&argc, &argv);
    gst_debug_set_default_threshold(GST_LEVEL_ERROR);
    gst_debug_set_threshold_for_name("libav", GST_LEVEL_NONE);
    qDebug() << "[main] GStreamer debug threshold set to ERROR, libav decoder spam suppressed.";
    gst_quality_monitor_register(NULL);
    qDebug() << "[main] GStreamer Initialized.";

    setupSslContext();

    qDebug() << "[main] Opening main window...";
    MainWindow w;
    w.show();

    qDebug() << "[main] Main window shown";
    qDebug() << "=================================";

    return a.exec();
}
