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
    setWindowTitle(QString(Constants::App::WINDOW_TITLE));
    resize(Constants::App::DEFAULT_WIDTH, Constants::App::DEFAULT_HEIGHT);

    qApp->installEventFilter(this);

    qDebug() << "[MainWindow] Initializing RosBridgeClient...";
    ConfigManager::instance().loadDefaults();
    m_isDark = ConfigManager::instance().getDarkTheme();
    m_rosClient = new RosBridgeClient(this);
    m_currentRobotWsUrl = ConfigManager::instance().getRobotIp();
    m_rosClient->connectToHost(m_currentRobotWsUrl);

    qDebug() << "[MainWindow] Initializing CameraControlClient...";
    m_cameraClient = new CameraControlClient(this);

    initUI();
    initConnections();
    updateTopBarUserInfo();

    loadTheme(m_isDark ? QStringLiteral("style/theme_dark.qss")
                       : QStringLiteral("style/theme_light.qss"));

    m_inputTimer = new QTimer(this);
    connect(m_inputTimer, &QTimer::timeout, this, &MainWindow::processInput);

    m_cameraTiltTimer = new QTimer(this);
    m_cameraTiltTimer->setInterval(100);
    connect(m_cameraTiltTimer, &QTimer::timeout, this, &MainWindow::processCameraTiltInput);

    connect(&ConfigManager::instance(), &ConfigManager::configChanged, this, &MainWindow::onConfigChanged);
    connect(&AuthManager::instance(), &AuthManager::userProfileResolved, this,
            [this](const QString &userId, const QString &email) {
                if (!userId.trimmed().isEmpty()
                    && userId.trimmed() == ConfigManager::instance().getActiveUserId()) {
                    ConfigManager::instance().setActiveUserEmail(email);
                }
            });
    connect(&AuthManager::instance(), &AuthManager::authenticationRequired, this,
            [this](const QString &message) {
                if (!reopenLoginDialog(message)) {
                    close();
                }
            });

    if (AuthManager::instance().isAuthenticated()) {
        AuthManager::instance().fetchCurrentUserProfile();
    }
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

bool MainWindow::handleWasdKey(QKeyEvent *event, bool isPress)
{
    if (m_robotMode != Sidebar::ManualMode) {
        return false;
    }
    if (!ConfigManager::instance().getManualControl()) {
        return false;
    }
    if (event->isAutoRepeat()) {
        return false;
    }

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
    stopManualMotion();
    clearAllGoalOverlays();

    if (m_livePage) {
        m_livePage->clearPathOverlay();
    }
    if (m_robotMode == Sidebar::PatrolMode && m_livePage) {
        m_livePage->clearPatrolOverlay();
    }
    if (m_robotMode == Sidebar::PatrolMode && m_sidebar) {
        m_sidebar->setPatrolAddPointActive(false);
    }

    clearGoalTracking();
    deactivateControlSession();

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
        return;
    }

    if (m_livePage) {
        m_livePage->clearVideoGoalOverlay();
    }
    if (m_fullPage) {
        m_fullPage->clearVideoGoalOverlay();
    }
}

void MainWindow::setSharedVideoGoalOverlay(int channelIndex,
                                           const QPointF &normalizedStart,
                                           const QPointF &normalizedEnd)
{
    m_hasSharedVideoGoalOverlay = true;
    m_sharedVideoGoalChannelIndex = channelIndex;
    m_sharedVideoGoalStartNormalized = normalizedStart;
    m_sharedVideoGoalEndNormalized = normalizedEnd;
    applySharedVideoGoalOverlay();
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

    const QJsonValue detectedValue = alertData.value(QStringLiteral("detected"));
    const bool detected = detectedValue.isBool()
        ? detectedValue.toBool()
        : (detectedValue.toString().trimmed().compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0);

    qDebug() << "[MainWindow] fall_alert received:" << QJsonDocument(alertData).toJson(QJsonDocument::Compact);

    if (!detected) {
        hideFallAlert();
        return;
    }

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
