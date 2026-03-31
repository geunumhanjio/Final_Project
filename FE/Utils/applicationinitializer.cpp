#include "applicationinitializer.h"
#include "configmanager.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <gst/gst.h>
#include "GstQualityMonitor.hpp"

bool ApplicationInitializer::initializeEnvironment()
{
    qDebug() << "=== VEDA CCTV System Starting ===";
    
    const bool opencvOk = initializeOpenCV();
    const bool gstreamerOk = initializeGStreamer();
    const bool fontsOk = initializeFonts();
    const bool sslOk = initializeSSL();
    
    return opencvOk && gstreamerOk && fontsOk && sslOk;
}

bool ApplicationInitializer::initializeGStreamer()
{
    qDebug() << "[ApplicationInitializer] Initializing GStreamer...";
    
    const QString binPath = findGStreamerBinPath();
    const QString rootPath = findGStreamerRootPath();
    
    if (binPath.isEmpty()) {
        qWarning() << "[ApplicationInitializer] No GStreamer runtime path found. Bundled playback/RTSP may fail.";
        return false;
    }
    
    if (!setupGStreamerEnvironment(binPath, rootPath)) {
        return false;
    }
    
    // Initialize GStreamer
    int argc = 0;
    char **argv = nullptr;
    gst_init(&argc, &argv);
    gst_debug_set_default_threshold(GST_LEVEL_ERROR);
    gst_debug_set_threshold_for_name("libav", GST_LEVEL_NONE);
    gst_quality_monitor_register(NULL);
    
    qDebug() << "[ApplicationInitializer] GStreamer Initialized.";
    return true;
}

bool ApplicationInitializer::initializeOpenCV()
{
    return setupOpenCVEnvironment();
}

bool ApplicationInitializer::initializeFonts()
{
    qDebug() << "[ApplicationInitializer] Loading application fonts...";
    
    const QStringList fontResources = {
        QStringLiteral(":/fonts/Pretendard-Regular.otf"),
        QStringLiteral(":/fonts/Pretendard-Medium.otf"),
        QStringLiteral(":/fonts/Pretendard-Bold.otf")
    };

    QString appFontFamily;
    for (const QString &fontPath : fontResources) {
        const int fontId = QFontDatabase::addApplicationFont(fontPath);
        if (fontId < 0) {
            qWarning() << "[ApplicationInitializer] Failed to load:" << fontPath;
            continue;
        }

        if (appFontFamily.isEmpty()) {
            appFontFamily = QFontDatabase::applicationFontFamilies(fontId).value(0);
        }
    }

    if (!appFontFamily.isEmpty()) {
        QApplication::setFont(QFont(appFontFamily));
        qDebug() << "[ApplicationInitializer] Loaded application font:" << appFontFamily;
        return true;
    } else {
        qWarning() << "[ApplicationInitializer] Pretendard font family could not be loaded.";
        return false;
    }
}

bool ApplicationInitializer::initializeSSL()
{
    qDebug() << "[ApplicationInitializer] Setting up SSL context...";
    
    const QString certResourcePath = ":/crt/env/server.crt";
    QFile certFile(certResourcePath);
    if (!certFile.open(QIODevice::ReadOnly)) {
        qWarning() << "[ApplicationInitializer] Failed to open embedded server.crt from resource.";
        return false;
    }

    const QByteArray certData = certFile.readAll();
    certFile.close();

    const QList<QSslCertificate> certs = QSslCertificate::fromData(certData);
    if (certs.isEmpty()) {
        qWarning() << "[ApplicationInitializer] Failed to parse embedded server.crt! RTSPS might fail unless tls-validation-flags=0 is used.";
        return false;
    }

    QSslConfiguration config = QSslConfiguration::defaultConfiguration();
    config.addCaCertificates(certs);
    QSslConfiguration::setDefaultConfiguration(config);
    qDebug() << "[ApplicationInitializer] Embedded RTSPS server certificate (server.crt) registered successfully.";
    
    return true;
}

QFont ApplicationInitializer::getApplicationFont()
{
    return QApplication::font();
}

QString ApplicationInitializer::findGStreamerBinPath()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString bundledGstRoot = QDir(appDir).filePath("gstreamer");
    const QString bundledGstBinPath = QDir(bundledGstRoot).filePath("bin");
    const QString bundledRootDll = QDir(appDir).filePath("gstreamer-1.0-0.dll");

    if (QFileInfo::exists(bundledRootDll) && QDir(bundledGstRoot).exists()) {
        qDebug() << "[ApplicationInitializer] Using app-local GStreamer DLLs from:" << appDir;
        return appDir;
    }
    
    if (QDir(bundledGstBinPath).exists()) {
        qDebug() << "[ApplicationInitializer] Using bundled GStreamer from:" << bundledGstBinPath;
        return bundledGstBinPath;
    }

#ifdef GST_BIN_PATH
    const QString configuredPath = QString::fromUtf8(GST_BIN_PATH);
    if (QDir(configuredPath).exists()) {
        QDir configuredBinDir(configuredPath);
        configuredBinDir.cdUp();
        qDebug() << "[ApplicationInitializer] Using configured GStreamer from:" << configuredPath;
        return configuredPath;
    }
#else
    qWarning() << "[ApplicationInitializer] GST_BIN_PATH not defined in CMakeLists.txt!";
#endif

    return QString();
}

QString ApplicationInitializer::findGStreamerRootPath()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString bundledGstRoot = QDir(appDir).filePath("gstreamer");
    const QString bundledGstBinPath = QDir(bundledGstRoot).filePath("bin");
    const QString bundledRootDll = QDir(appDir).filePath("gstreamer-1.0-0.dll");

    if (QFileInfo::exists(bundledRootDll) && QDir(bundledGstRoot).exists()) {
        return bundledGstRoot;
    }
    
    if (QDir(bundledGstBinPath).exists()) {
        return bundledGstRoot;
    }

#ifdef GST_BIN_PATH
    const QString configuredPath = QString::fromUtf8(GST_BIN_PATH);
    if (QDir(configuredPath).exists()) {
        QDir configuredBinDir(configuredPath);
        configuredBinDir.cdUp();
        return configuredBinDir.absolutePath();
    }
#endif

    return QString();
}

bool ApplicationInitializer::setupGStreamerEnvironment(const QString &binPath, const QString &rootPath)
{
    QString gstBinPath = QDir::toNativeSeparators(binPath);
    QByteArray currentPath = qgetenv("PATH");
    QByteArray newPath = gstBinPath.toLocal8Bit() + ";" + currentPath;
    qputenv("PATH", newPath);

    qDebug() << "[ApplicationInitializer] GStreamer bin path:" << gstBinPath;

    QString gstRootPath = rootPath;
    if (gstRootPath.isEmpty()) {
        QDir binDir(gstBinPath);
        binDir.cdUp();
        gstRootPath = binDir.absolutePath();
    }

    const QString pluginPath = QDir(gstRootPath).filePath("lib/gstreamer-1.0");
    qputenv("GST_PLUGIN_PATH", QDir::toNativeSeparators(pluginPath).toLocal8Bit());
    qDebug() << "[ApplicationInitializer] GST_PLUGIN_PATH set to:" << pluginPath;

    const QString scannerPath = QDir(gstRootPath).filePath("libexec/gstreamer-1.0/gst-plugin-scanner.exe");
    if (QFileInfo::exists(scannerPath)) {
        qputenv("GST_PLUGIN_SCANNER", QDir::toNativeSeparators(scannerPath).toLocal8Bit());
        qDebug() << "[ApplicationInitializer] GST_PLUGIN_SCANNER set to:" << scannerPath;
    }
    
    qputenv("GST_DEBUG", "1,libav:0");
    qDebug() << "[ApplicationInitializer] GST_DEBUG configured to: 1,libav:0";
    
    return true;
}

bool ApplicationInitializer::setupOpenCVEnvironment()
{
    const QString opencvBinPath = QString::fromUtf8(OPENCV_BIN_PATH);
    
    if (opencvBinPath.isEmpty() || !QDir(opencvBinPath).exists()) {
        qDebug() << "[ApplicationInitializer] OpenCV bin path not found or empty";
        return false;
    }
    
    const QString nativeOpenCvPath = QDir::toNativeSeparators(opencvBinPath);
    const QByteArray currentPath = qgetenv("PATH");
    if (!currentPath.contains(nativeOpenCvPath.toLocal8Bit())) {
        qputenv("PATH", nativeOpenCvPath.toLocal8Bit() + ";" + currentPath);
    }
    qDebug() << "[ApplicationInitializer] OpenCV bin path:" << nativeOpenCvPath;
    
    return true;
}
