#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QCloseEvent>
#include "topbar.h"
#include "sidebar.h"
#include "liveview.h"
#include "rosbridgeclient.h"
#include "cameracontrolclient.h" // [New]
#include <QSet>
#include <QTimer>
#include <QKeyEvent>

QT_BEGIN_NAMESPACE
QT_END_NAMESPACE

#include "videowidget.h"
#include "playbackview.h" // [New]

class SettingsWidget;
class FullScreenView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    void processInput();

private:
    TopBar *m_topBar;
    Sidebar *m_sidebar;
    QStackedWidget *m_centralStack; // 페이지 전환 컨테이너
    LiveView *m_livePage;
    FullScreenView *m_fullPage;
    PlaybackView *m_playbackPage;   // [Modified] VideoWidget -> PlaybackView
    SettingsWidget *m_settingsPage; // Settings Widget

    void initUI();          // UI 초기화
    void initConnections(); // 시그널/슬롯 연결
    void toggleTheme();     // Theme toggle method
    void loadTheme(const QString &path); // Robust theme loader

    
private:
    bool m_isDark; // Current theme state
    
    // ROS2 Control
    RosBridgeClient *m_rosClient;
    CameraControlClient *m_cameraClient;
    
    QTimer *m_inputTimer;
    QSet<int> m_pressedKeys;
    
    // [New] Widget to return to after closing FullScreenView
    QWidget* m_returnToWidget = nullptr; 
};
#endif // MAINWINDOW_H
