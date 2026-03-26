#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QCloseEvent>
#include "topbar.h"
#include "sidebar.h"
#include "liveview.h"
#include "playbackview.h"
#include "rosbridgeclient.h"
#include "cameracontrolclient.h"
#include <QSet>
#include <QTimer>
#include <QKeyEvent>
#include <QPointF>
#include <QString>

QT_BEGIN_NAMESPACE
QT_END_NAMESPACE

class QJsonObject;
class FrucVideoProcessor;
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
    bool eventFilter(QObject *obj, QEvent *event) override; 
    void keyPressEvent(QKeyEvent *event) override;   
    void keyReleaseEvent(QKeyEvent *event) override; 

private:
    bool handleWasdKey(QKeyEvent *event, bool isPress);
    bool handleCameraTiltKey(QKeyEvent *event, bool isPress);
    void processCameraTiltInput();
    void stopCameraTiltInput();
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
    void finalizePatrolPath();
    bool reopenLoginDialog(const QString &message = QString());
    void startFrucProcessing(const QString &filePath);
    void stopBackgroundWorkers();
    void updateTopBarUserInfo();
    void promptLogout();
    void deactivateControlSession();
    void updateSidebarForCurrentPage();
    void applySharedVideoGoalOverlay();
    void setSharedVideoGoalOverlay(int channelIndex, const QPointF &normalizedStart, const QPointF &normalizedEnd);
    void clearSharedVideoGoalOverlay();

private slots:
    void processInput();
    void onConfigChanged();

private:
    TopBar *m_topBar;
    Sidebar *m_sidebar;
    QStackedWidget *m_centralStack;
    LiveView *m_livePage;
    FullScreenView *m_fullPage;
    PlaybackView *m_playbackPage;
    SettingsWidget *m_settingsPage;

    RosBridgeClient *m_rosClient;
    CameraControlClient *m_cameraClient;

    void initUI();
    void initConnections();
    void toggleTheme();
    void loadTheme(const QString &path);

private:
    bool m_isDark;

    QTimer *m_inputTimer;
    QTimer *m_cameraTiltTimer = nullptr;
    QSet<int> m_pressedKeys;
    QSet<int> m_pressedTiltKeys;
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
    bool m_skipCloseConfirmation = false;
    QSet<QString> m_pendingFrucJobs;
    QSet<FrucVideoProcessor *> m_activeFrucProcessors;
    double m_cameraTiltAngle = 0.0;

    QWidget *m_returnToWidget = nullptr;
};
#endif // MAINWINDOW_H
