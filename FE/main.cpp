/**
 * @file main.cpp
 * @brief 프로그램의 진입점. GStreamer 환경 변수 설정 및 메인 윈도우 실행.
 */
#include "mainwindow.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <gst/gst.h> // [New] Needed for gst_init
#include <QSslConfiguration>
#include <QSslCertificate>
#include <QFileInfo>
#include "Video/Gst/GstQualityMonitor.hpp"

/**
 * @brief [New] RTSPS 서버 인증서 등록
 * 서버의 server.crt를 신뢰할 수 있는 CA 목록에 추가합니다.
 */
void setupSslContext() {
    // 1. 프로젝트 폴더의 env/server.crt 경로 설정
    // 실행 파일 위치 기준으로 프로젝트 소스 루트의 env 폴더를 찾습니다.
    QString appDir = QCoreApplication::applicationDirPath();
    QString certPath = QDir(appDir).filePath("env/server.crt");
    
    // 개발 환경(build 폴더) 고려: 한 단계 위 확인
    if (!QFile::exists(certPath)) {
        certPath = QDir(appDir).filePath("../env/server.crt");
    }
    
    // 절대 경로 디버그 출력 (절대경로 하드코딩 방지)
    qDebug() << "[SSL] Checking certificate at:" << QFileInfo(certPath).absoluteFilePath();

    // 2. 인증서 로드
    QList<QSslCertificate> certs = QSslCertificate::fromPath(certPath);

    if (!certs.isEmpty()) {
        // 3. 기존 기본 설정에 추가
        QSslConfiguration config = QSslConfiguration::defaultConfiguration();
        config.addCaCertificates(certs);
        QSslConfiguration::setDefaultConfiguration(config);
        qDebug() << "✅ [SSL] RTSPS Server Certificate (server.crt) registered successfully.";
    } else {
        qWarning() << "❌ [SSL] Failed to load server.crt! RTSPS might fail unless tls-validation-flags=0 is used.";
    }
}

int main(int argc, char *argv[])
{
    qDebug() << "=== VEDA CCTV System Starting ===";

    // [중요] CMakeLists.txt에서 찾아낸 GStreamer 'bin' 폴더 경로를 확인합니다.
#ifdef GST_BIN_PATH
    QString gstBinPath = QString::fromUtf8(GST_BIN_PATH);
    gstBinPath = QDir::toNativeSeparators(gstBinPath);
    QByteArray currentPath = qgetenv("PATH");
    QByteArray newPath = gstBinPath.toLocal8Bit() + ";" + currentPath;
    qputenv("PATH", newPath);

    qDebug() << "[main] GStreamer bin path:" << gstBinPath;

    // 플러그인 경로도 명시적으로 지정
    QDir binDir(gstBinPath);
    binDir.cdUp(); // 상위 폴더로 이동 (mingw_x86_64)
    QString libPath = binDir.absolutePath() + "\\lib\\gstreamer-1.0";
    qputenv("GST_PLUGIN_PATH", libPath.toLocal8Bit());
    qDebug() << "[main] GST_PLUGIN_PATH set to:" << libPath;
#else
    qWarning() << "[main] GST_BIN_PATH not defined in CMakeLists.txt!";
#endif

    // GStreamer 디버그 레벨 설정
    // 0: 없음, 1: 에러만, 2: 경고+에러, 3: 정보+경고+에러, 4: 디버그, 5: 로그
    qputenv("GST_DEBUG", "3");
    qDebug() << "[main] GST_DEBUG level set to: 3";

    // Initialize GStreamer Environment
    // gst_init must be called before any GStreamer usage.
    // Passing nullptr allowing GStreamer to parse standard command line args if needed (none passed)
    qDebug() << "[main] Initializing GStreamer...";
    gst_init(&argc, &argv); 
    
    // Register Custom Element
    gst_quality_monitor_register(NULL);
    
    qDebug() << "[main] GStreamer Initialized.";

    // [New] Setup SSL configuration for RTSPS
    setupSslContext();

    // GStreamer 초기화 전에 환경 확인
    QApplication a(argc, argv);

    qDebug() << "[main] Qt application created";

    // Main window will load the theme
    // qApp->setStyleSheet(...) moved to MainWindow

    qDebug() << "[main] Opening main window...";

    MainWindow w;
    w.show();

    qDebug() << "[main] Main window shown";
    qDebug() << "=================================";

    return a.exec();
}
