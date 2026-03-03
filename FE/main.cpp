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
#include "Video/Gst/GstQualityMonitor.hpp"

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
