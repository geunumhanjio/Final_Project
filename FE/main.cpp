/**
 * @file main.cpp
 * @brief Program entry point. Initializes Qt, GStreamer, SSL, and opens the main window.
 */
#include "mainwindow.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
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

    QApplication a(argc, argv);
    qDebug() << "[main] Qt application created";

    qDebug() << "[main] Initializing GStreamer...";
    gst_init(&argc, &argv);
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
