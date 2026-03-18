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
#include <QPointF>
#include <QString>

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
    bool eventFilter(QObject *obj, QEvent *event) override; // [New]
    void keyPressEvent(QKeyEvent *event) override;   // Kept for backup/other keys
    void keyReleaseEvent(QKeyEvent *event) override; // Kept for backup/other keys

private:
    bool handleWasdKey(QKeyEvent *event, bool isPress); // [New] Shared logic
    void applyRobotMode(Sidebar::RobotMode mode);
    void syncRobotModeToBackend(bool sendIdleMotion = false);
    void stopManualMotion();
    void requestEmergencyStop();
    void clearAllGoalOverlays();
    void armGoalTracking(const QPointF &goalPosition, double goalYaw);
    void clearGoalTracking();
    void handleGoalOdomUpdate(const QJsonObject &data);
    void handleGoalNavStatus(const QJsonObject &data);
    void handleGoalNavFeedback(const QJsonObject &data);
    void deactivateControlSession();
    void updateSidebarForCurrentPage();
    void applySharedVideoGoalOverlay();
    void setSharedVideoGoalOverlay(int channelIndex, const QPointF &normalizedStart, const QPointF &normalizedEnd);
    void clearSharedVideoGoalOverlay();

private slots:
    void processInput();
    void onConfigChanged(); // [New]

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
    QString m_currentRobotWsUrl;
    Sidebar::RobotMode m_robotMode = Sidebar::ManualMode;
    bool m_sidebarVisibleBeforeFullScreen = true;
    bool m_sidebarForcedHiddenForFullScreen = false;
    bool m_hasSharedVideoGoalOverlay = false;
    int m_sharedVideoGoalChannelIndex = -1;
    QPointF m_sharedVideoGoalStartNormalized;
    QPointF m_sharedVideoGoalEndNormalized;
    bool m_isClosingFullScreen = false;
    bool m_hasActiveGoal = false;
    QPointF m_activeGoalPosition;
    double m_activeGoalYaw = 0.0;
    int m_goalArrivalStableCount = 0;
    
    // [New] Widget to return to after closing FullScreenView
    QWidget* m_returnToWidget = nullptr; 
};
#endif // MAINWINDOW_H
