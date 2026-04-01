#include "mainwindow.h"

#include "authmanager.h"
#include "configmanager.h"
#include "fullscreenview.h"
#include "jsonutils.h"
#include "constants.h"

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlag(Qt::FramelessWindowHint, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setWindowTitle(QString(Constants::App::WINDOW_TITLE));
    resize(Constants::App::DEFAULT_WIDTH, Constants::App::DEFAULT_HEIGHT);

    qApp->installEventFilter(this);

    qDebug() << "[MainWindow] Initializing RosBridgeClient...";
    ConfigManager::instance().loadDefaults();
    m_isDark = ConfigManager::instance().getDarkTheme();
    m_rosClient = new RosBridgeClient(this);
    m_rosClient->connectToHost(ConfigManager::instance().getRobotIp()); // [Modified] Use config
    
    qDebug() << "[MainWindow] Initializing CameraControlClient...";
    m_cameraClient = new CameraControlClient(this);

    initUI();
    initConnections();
    updateTopBarUserInfo();

    loadTheme(m_isDark ? QStringLiteral("style/theme_dark.qss")
                       : QStringLiteral("style/theme_light.qss"));

    m_inputTimer = new QTimer(this);
    connect(m_inputTimer, &QTimer::timeout, this, &MainWindow::processInput);
    
    // Listen to config changes
    connect(&ConfigManager::instance(), &ConfigManager::configChanged, this, &MainWindow::onConfigChanged);
}

MainWindow::~MainWindow()
{
    stopBackgroundWorkers();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        const int key = keyEvent->key();

        QWidget *focusWidget = QApplication::focusWidget();
        if (focusWidget) {
            if (qobject_cast<QLineEdit *>(focusWidget)
                || qobject_cast<QTextEdit *>(focusWidget)
                || qobject_cast<QPlainTextEdit *>(focusWidget)) {
                return false;
            }
        }

        if (key == Qt::Key_W || key == Qt::Key_S || key == Qt::Key_A || key == Qt::Key_D) {
            return handleWasdKey(keyEvent, event->type() == QEvent::KeyPress);
        }

        if (key == Qt::Key_Up || key == Qt::Key_Down) {
            return handleCameraTiltKey(keyEvent, event->type() == QEvent::KeyPress);
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

// [New] Shared WASD Logic
bool MainWindow::handleWasdKey(QKeyEvent *event, bool isPress) {
    if (!ConfigManager::instance().getManualControl()) return false;
    if (event->isAutoRepeat()) return false;
    
    int key = event->key();

    const int key = event->key();
    if (isPress) {
        if (!m_pressedKeys.contains(key)) {
            m_pressedKeys.insert(key);
            if (!m_inputTimer->isActive()) {
                m_inputTimer->start(100);
                processInput();
            }
        }
    } else if (m_pressedKeys.contains(key)) {
        m_pressedKeys.remove(key);
        if (m_pressedKeys.isEmpty()) {
            m_inputTimer->stop();
            if (m_rosClient) {
                m_rosClient->sendCmdVel(0.0, 0.0);
            }
            qDebug() << "[Control] STOP";
        } else {
            processInput();
        }
    }

    return true;
}

bool MainWindow::handleCameraTiltKey(QKeyEvent *event, bool isPress)
{
    if (!m_rosClient) {
        return false;
    }
    if (event->isAutoRepeat()) {
        return true;
    }

    const int key = event->key();
    if (isPress) {
        if (!m_pressedTiltKeys.contains(key)) {
            m_pressedTiltKeys.insert(key);
            if (!m_cameraTiltTimer->isActive()) {
                m_cameraTiltTimer->start();
                processCameraTiltInput();
            }
        }
    } else if (m_pressedTiltKeys.contains(key)) {
        m_pressedTiltKeys.remove(key);
        if (m_pressedTiltKeys.isEmpty()) {
            stopCameraTiltInput();
        } else {
            processCameraTiltInput();
        }
    }

    return true;
}

void MainWindow::processCameraTiltInput()
{
    if (!m_rosClient || !m_cameraTiltTimer) {
        return;
    }

    int direction = 0;
    if (m_pressedTiltKeys.contains(Qt::Key_Up)) {
        direction -= 1;
    }
    if (m_pressedTiltKeys.contains(Qt::Key_Down)) {
        direction += 1;
    }

    if (direction == 0) {
        return;
    }

    constexpr double tiltStep = 5.0;
    m_cameraTiltAngle += tiltStep * static_cast<double>(direction);

    if (m_cameraTiltAngle < 0.0) {
        m_cameraTiltAngle = 0.0;
    } else if (m_cameraTiltAngle > 180.0) {
        m_cameraTiltAngle = 180.0;
    }

    m_rosClient->sendCameraTilt(m_cameraTiltAngle);
    qDebug() << "[CameraTilt] angle:" << m_cameraTiltAngle;
}

void MainWindow::stopCameraTiltInput()
{
    m_pressedTiltKeys.clear();
    if (m_cameraTiltTimer) {
        m_cameraTiltTimer->stop();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    QMainWindow::keyPressEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateFallAlertPosition();
    if (m_fallAlertPanel && m_fallAlertPanel->isVisible()) {
        m_fallAlertPanel->raise();
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::processInput()
{
    double linear = 0.0;
    double angular = 0.0;
    const double linearStep = ConfigManager::instance().getManualLinearX();
    const double angularStep = ConfigManager::instance().getManualAngularZ();

    if (m_pressedKeys.contains(Qt::Key_W)) {
        linear += linearStep;
    }
    if (m_pressedKeys.contains(Qt::Key_S)) {
        linear -= linearStep;
    }
    if (m_pressedKeys.contains(Qt::Key_A)) {
        angular += angularStep;
    }
    if (m_pressedKeys.contains(Qt::Key_D)) {
        angular -= angularStep;
    }

    if (m_rosClient) {
        m_rosClient->sendCmdVel(linear, angular);
    }
}

void MainWindow::syncRobotModeToBackend(bool sendIdleMotion)
{
    if (!m_rosClient) {
        return;
    }

    const bool isManualMode = (m_robotMode == Sidebar::ManualMode);
    const bool isTrackingMode = (m_robotMode == Sidebar::AutoMode);
    m_rosClient->sendModeControl(isManualMode ? QStringLiteral("manual")
                                              : QStringLiteral("auto"));
    m_rosClient->sendTrackingEnable(isTrackingMode);

    if (sendIdleMotion && isManualMode) {
        m_rosClient->sendCmdVel(0.0, 0.0);
    }
}

void MainWindow::stopManualMotion()
{
    m_pressedKeys.clear();
    if (m_inputTimer) {
        m_inputTimer->stop();
    }
    if (m_rosClient) {
        m_rosClient->sendCmdVel(0.0, 0.0);
    }
}

void MainWindow::requestEmergencyStop()
{
    qDebug() << "[MainWindow] initConnections Started...";
    
    // [New] Connect STREAM_STATS from WebSocket to UI components
    if (m_cameraClient) {
        connect(m_cameraClient, &CameraControlClient::streamStatsReceived, this, [=](int channelId, double fps, double bitrateKbps, double proxyLatencyMs){
            if (m_livePage) m_livePage->updateStreamStats(channelId, fps, bitrateKbps, proxyLatencyMs);
            if (m_fullPage) m_fullPage->updateStreamStats(channelId, fps, bitrateKbps, proxyLatencyMs);
        });
        
        // [New] Request STREAM_STATS on OSD Check
        connect(m_livePage, &LiveView::streamStatsRequested, [=](int channelId, bool start) {
            ConfigManager::instance().loadDefaults();
            QString ip = ConfigManager::instance().getCameraIp();
            m_cameraClient->requestStreamStats(ip, channelId, start);
        });
        connect(m_fullPage, &FullScreenView::streamStatsRequested, [=](int channelId, bool start) {
            ConfigManager::instance().loadDefaults();
            QString ip = ConfigManager::instance().getCameraIp();
            m_cameraClient->requestStreamStats(ip, channelId, start);
        });
    }

    connect(m_topBar, &TopBar::sidebarToggled, [=](){ m_sidebar->setVisible(!m_sidebar->isVisible()); });
    connect(m_topBar, &TopBar::modeChanged, [=](int index){
        m_centralStack->setCurrentIndex(index);
        // [New] Switch Sidebar Context
        // Index 0: Live -> SidebarMode::Live
        // Index 1: Playback -> SidebarMode::Playback
        // Others: Maybe default to Live or Hide?
        if (index == 1) {
            m_sidebar->setMode(Sidebar::Playback);
        } else {
            m_sidebar->setMode(Sidebar::Live);
        }
    });
    connect(m_topBar, &TopBar::themeToggled, this, &MainWindow::toggleTheme);
    connect(m_sidebar, &Sidebar::channelStateChanged, m_livePage, &LiveView::setChannelVisible);
    
    // [New] Sidebar Category Selection -> Filter Playback List
    connect(m_sidebar, &Sidebar::categorySelected, m_playbackPage, &PlaybackView::filterRecordings);
    
    // Connect Recording Navigation
    connect(m_livePage, &LiveView::recordCommandRequested, [=](int channelId, bool start){
        // 1. Get Camera IP (Assume all cameras share the same NVR/IP for now as per ConfigManager)
        ConfigManager::instance().loadDefaults();
        QString ip = ConfigManager::instance().getCameraIp();

        // 2. Send Command to Camera Server (Port 9000)
        m_cameraClient->sendRecordCommand(ip, channelId, start);
        
        // 3. Feedback
        if (start) {
            qDebug() << "[Recording] Started on Channel" << channelId;
        } else {
            qDebug() << "[Recording] Stopped on Channel" << channelId;
            // Note: Playback update is now handled by videoReceived signal
        }
    });

    // [New] Connect Playback View
    if (m_cameraClient) {
        // 1. Refresh Button -> Request Recordings
        connect(m_playbackPage, &PlaybackView::refreshRequested, [=](){
             ConfigManager::instance().loadDefaults();
             QString ip = ConfigManager::instance().getCameraIp();
             m_cameraClient->requestRecordings(ip);
        });

        // 2. Received List -> Update UI
        connect(m_cameraClient, &CameraControlClient::recordingListReceived, m_playbackPage, &PlaybackView::updateList);

        // [New] Video Received (Recording Finished) -> Auto Refresh
        connect(m_cameraClient, &CameraControlClient::videoReceived, [=](QString url){
            qDebug() << "[MainWindow] Recording finished at:" << url << "- Refreshing list.";
            if (m_playbackPage) emit m_playbackPage->refreshRequested();
        });

        // 3. Play Video (Check Local -> Download -> Play)
        connect(m_playbackPage, &PlaybackView::playRequested, [=](const QString &filename){
            qDebug() << "[MainWindow] playRequested signal received for:" << filename;
            QString savePath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
            QString localFilePath = savePath + "/" + filename;
            
            QFileInfo fileInfo(localFilePath);
            if (fileInfo.exists() && fileInfo.size() > 1024) {
                qDebug() << "[MainWindow] Found valid local file:" << localFilePath << "Size:" << fileInfo.size();
                
                // [Mod] Keep background RTSP streams running (No stopAll)
                // m_livePage->stopAll(); 

                m_returnToWidget = m_playbackPage; // [Mod] Return to PlaybackView on close
                m_fullPage->play(localFilePath, 0); 
                m_centralStack->setCurrentWidget(m_fullPage);
            } else {
                if (fileInfo.exists()) {
                     qDebug() << "[MainWindow] Found invalid/small file (" << fileInfo.size() << "bytes). Deleting to re-download.";
                     QFile::remove(localFilePath);
                }
                qDebug() << "[MainWindow] File not found local, requesting download:" << filename;
                ConfigManager::instance().loadDefaults();
                QString ip = ConfigManager::instance().getCameraIp();
                m_cameraClient->requestDownload(ip, filename);
            }
        });
        
        // 4. Download Progress
        connect(m_cameraClient, &CameraControlClient::downloadProgress, m_playbackPage, &PlaybackView::updateDownloadProgress);
        
        // 5. Download Finished -> Add to List (No Auto Play)
        connect(m_cameraClient, &CameraControlClient::downloadFinished, [=](const QString &filePath){
            qDebug() << "[MainWindow] Download Finished:" << filePath;
            m_playbackPage->addLocalItem(filePath);
        });

    if (m_rosClient) {
        m_rosClient->sendNavCancel();
        m_rosClient->sendCmdVel(0.0, 0.0);
    }

    qDebug() << "[MainWindow] Emergency stop requested - cleared goal overlays and canceled active navigation.";
}

void MainWindow::clearAllGoalOverlays()
{
    clearSharedVideoGoalOverlay();
    if (m_livePage) {
        m_livePage->clearGoalOverlays();
    }
    if (m_fullPage) {
        m_fullPage->clearGoalOverlay();
    }
}

void MainWindow::armGoalTracking(const QPointF &goalPosition, double goalYaw)
{
    m_hasActiveGoal = true;
    m_activeGoalPosition = goalPosition;
    m_activeGoalYaw = goalYaw;
    m_goalArrivalStableCount = 0;
}

void MainWindow::clearGoalTracking()
{
    m_hasActiveGoal = false;
    m_activeGoalPosition = QPointF();
    m_activeGoalYaw = 0.0;
    m_goalArrivalStableCount = 0;
}

void MainWindow::handleGoalOdomUpdate(const QJsonObject &data)
{
    if (!m_hasActiveGoal) {
        return;
    }

    bool ok = false;
    const QPointF robotPosition = JsonUtils::extractPoint(data, &ok);
    if (!ok) {
        return;
    }

    const double distance = std::hypot(robotPosition.x() - m_activeGoalPosition.x(),
                                       robotPosition.y() - m_activeGoalPosition.y());
    const double linearSpeed = JsonUtils::extractLinearSpeed(data);
    const double angularSpeed = JsonUtils::extractAngularSpeed(data);
    constexpr double kGoalDistanceToleranceMeters = 0.20;
    constexpr double kGoalLinearSpeedTolerance = 0.05;
    constexpr double kGoalAngularSpeedTolerance = 0.10;
    constexpr int kRequiredStableSamples = 3;

    if (distance <= kGoalDistanceToleranceMeters
        && linearSpeed <= kGoalLinearSpeedTolerance
        && angularSpeed <= kGoalAngularSpeedTolerance) {
        ++m_goalArrivalStableCount;
    } else {
        m_goalArrivalStableCount = 0;
    }

    if (m_goalArrivalStableCount >= kRequiredStableSamples) {
        qDebug() << "[MainWindow] Goal reached by odom -> pos:" << robotPosition
                 << "goal:" << m_activeGoalPosition
                 << "linearSpeed:" << linearSpeed
                 << "angularSpeed:" << angularSpeed;
        clearAllGoalOverlays();
        clearGoalTracking();
    }
}

void MainWindow::handleGoalNavStatus(const QJsonObject &data)
{
    if (!m_hasActiveGoal || !JsonUtils::hasSuccessStatus(data)) {
        return;
    }

    qDebug() << "[MainWindow] Goal reached by nav_status:"
             << QJsonDocument(data).toJson(QJsonDocument::Compact);
    clearAllGoalOverlays();
    clearGoalTracking();
}

void MainWindow::handleGoalNavFeedback(const QJsonObject &data)
{
    if (!m_hasActiveGoal || !JsonUtils::hasSuccessStatus(data)) {
        return;
    }

    qDebug() << "[MainWindow] Goal reached by nav_feedback:"
             << QJsonDocument(data).toJson(QJsonDocument::Compact);
    clearAllGoalOverlays();
    clearGoalTracking();
}

void MainWindow::finalizePatrolPath()
{
    if (!m_rosClient || m_robotMode != Sidebar::PatrolMode || !m_livePage) {
        return;
    }

    const QVector<QPointF> points = m_livePage->patrolPoints();
    if (points.isEmpty()) {
        return;
    }

    m_livePage->setPatrolAddPointMode(false);
    if (m_sidebar) {
        m_sidebar->setPatrolAddPointActive(false);
    }

    m_rosClient->sendNavQueue(points);
}

void MainWindow::applySharedVideoGoalOverlay()
{
    if (m_hasSharedVideoGoalOverlay) {
        if (m_livePage) {
            m_livePage->setVideoGoalOverlay(m_sharedVideoGoalChannelIndex,
                                            m_sharedVideoGoalStartNormalized,
                                            m_sharedVideoGoalEndNormalized);
        }
        if (m_fullPage) {
            m_fullPage->setVideoGoalOverlay(m_sharedVideoGoalChannelIndex,
                                            m_sharedVideoGoalStartNormalized,
                                            m_sharedVideoGoalEndNormalized);
        }
    });

    // [New] Connect Goal Pose
    connect(m_fullPage, &FullScreenView::reqGoalPose, [=](double x, double y, double theta){
        if (m_rosClient) {
            qDebug() << "[MainWindow] Sending Goal Pose -> x:" << x << "y:" << y << "theta:" << theta;
            m_rosClient->sendGoalPose(x, y, theta);
        }
    });

    qDebug() << "[MainWindow] initConnections Completed.";
}

void MainWindow::clearSharedVideoGoalOverlay()
{
    m_hasSharedVideoGoalOverlay = false;
    m_sharedVideoGoalChannelIndex = -1;
    m_sharedVideoGoalStartNormalized = QPointF();
    m_sharedVideoGoalEndNormalized = QPointF();
    applySharedVideoGoalOverlay();
}

void MainWindow::showFallAlert(const QJsonObject &data)
{
    const QJsonObject alertData = data.value(QStringLiteral("data")).isObject()
        ? data.value(QStringLiteral("data")).toObject()
        : data;

    if (!alertData.contains(QStringLiteral("detected"))) {
        return;
    }

    const QJsonValue detectedValue = alertData.value(QStringLiteral("detected"));
    const bool detected = detectedValue.isBool()
        ? detectedValue.toBool()
        : (detectedValue.toString().trimmed().compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0);

    if (!detected) {
        hideFallAlert();
        return;
    }

    qDebug() << "[MainWindow] fall_alert detected:" << QJsonDocument(alertData).toJson(QJsonDocument::Compact);

    const double angleDeg = alertData.value(QStringLiteral("angle_deg")).toDouble();
    const double timestampSeconds = alertData.value(QStringLiteral("timestamp")).toDouble();

    QDateTime alertTime = timestampSeconds > 0.0
        ? QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(std::llround(timestampSeconds * 1000.0)))
        : QDateTime();
    if (!alertTime.isValid()) {
        alertTime = QDateTime::currentDateTime();
    }

    if (m_topBar) {
        m_topBar->appendFallAlertLog(
            QStringLiteral("%1 | Angle %2 deg | 쓰러짐 감지")
                .arg(alertTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
                .arg(QString::number(angleDeg, 'f', 1)));
    }

    hideFallAlert();
}

void MainWindow::hideFallAlert()
{
    if (m_fallAlertPanel) {
        m_fallAlertPanel->hide();
    }
}

void MainWindow::updateFallAlertPosition()
{
    if (!m_fallAlertPanel || !m_centralStack) {
        return;
    }

    const int rightMargin = 24;
    const int topMargin = 24;
    const int x = qMax(0, m_centralStack->width() - m_fallAlertPanel->width() - rightMargin);
    m_fallAlertPanel->move(x, topMargin);
}

void MainWindow::deactivateControlSession()
{
    if (m_sidebar) {
        m_sidebar->setControlButtonActive(false);
    }
    if (m_livePage) {
        m_livePage->setGoalTargetingEnabled(false);
    }
    if (m_fullPage) {
        m_fullPage->setControlModeChecked(false);
        m_fullPage->setControlModeAvailable(m_robotMode == Sidebar::ControlMode);
    }
}

void MainWindow::updateSidebarForCurrentPage()
{
    if (!m_sidebar || !m_centralStack || !m_fullPage) {
        return;
    }

    const bool isFullScreenPage = (m_centralStack->currentWidget() == m_fullPage);
    if (isFullScreenPage) {
        if (!m_sidebarForcedHiddenForFullScreen) {
            m_sidebarVisibleBeforeFullScreen = m_sidebar->isVisible();
            m_sidebarForcedHiddenForFullScreen = true;
        }

        if (m_sidebar->isVisible()) {
            m_sidebar->hide();
        }
        return;
    }

    if (m_sidebarForcedHiddenForFullScreen) {
        m_sidebar->setVisible(m_sidebarVisibleBeforeFullScreen);
        m_sidebarForcedHiddenForFullScreen = false;
    }
}

void MainWindow::applyRobotMode(Sidebar::RobotMode mode)
{
    m_robotMode = mode;

    if (m_robotMode != Sidebar::ManualMode) {
        stopManualMotion();
    }
    syncRobotModeToBackend(m_robotMode == Sidebar::ManualMode);

    clearAllGoalOverlays();
    const bool controlActive = (m_robotMode == Sidebar::ControlMode) && m_sidebar->isControlButtonActive();
    m_livePage->setGoalTargetingEnabled(controlActive);
    m_livePage->setPatrolPlanningEnabled(m_robotMode == Sidebar::PatrolMode);

    if (m_robotMode != Sidebar::PatrolMode) {
        m_livePage->clearPatrolOverlay();
        m_livePage->setPatrolAddPointMode(false);
        if (m_sidebar) {
            m_sidebar->setPatrolPointCount(0);
            m_sidebar->setPatrolAddPointActive(false);
        }
    }

    m_fullPage->setControlModeAvailable(m_robotMode == Sidebar::ControlMode);
    m_fullPage->setRobotModeSelection(static_cast<int>(m_robotMode));
}

void MainWindow::onConfigChanged() {
    QString newIp = ConfigManager::instance().getRobotIp();
    // Reconnect ROS2 Client if IP Changed
    qDebug() << "[MainWindow] Config changed. Updating Robot IP to:" << newIp;
    m_rosClient->disconnect();
    m_rosClient->connectToHost(newIp);
}
