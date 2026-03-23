#include "mainwindow.h"

#include "authmanager.h"
#include "configmanager.h"
#include "fullscreenview.h"

#include <QApplication>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <cmath>

namespace {

QString ownedTrimmedString(const QJsonValue &value)
{
    return value.toString().trimmed();
}

QPointF extractPoint(const QJsonObject &data, bool *ok = nullptr)
{
    if (ok) {
        *ok = false;
    }

    const QJsonObject position = data.value(QStringLiteral("position")).toObject();
    if (!position.isEmpty()) {
        if (ok) {
            *ok = position.contains(QStringLiteral("x")) || position.contains(QStringLiteral("y"));
        }
        return QPointF(position.value(QStringLiteral("x")).toDouble(),
                       position.value(QStringLiteral("y")).toDouble());
    }

    const QJsonObject translation = data.value(QStringLiteral("translation")).toObject();
    if (!translation.isEmpty()) {
        if (ok) {
            *ok = translation.contains(QStringLiteral("x")) || translation.contains(QStringLiteral("y"));
        }
        return QPointF(translation.value(QStringLiteral("x")).toDouble(),
                       translation.value(QStringLiteral("y")).toDouble());
    }

    const QJsonObject pose = data.value(QStringLiteral("pose")).toObject();
    if (!pose.isEmpty()) {
        return extractPoint(pose, ok);
    }

    const QJsonObject transform = data.value(QStringLiteral("transform")).toObject();
    if (!transform.isEmpty()) {
        return extractPoint(transform, ok);
    }

    const QJsonObject wrapperMsg = data.value(QStringLiteral("msg")).toObject();
    if (!wrapperMsg.isEmpty()) {
        return extractPoint(wrapperMsg, ok);
    }

    const QJsonObject wrapperData = data.value(QStringLiteral("data")).toObject();
    if (!wrapperData.isEmpty()) {
        return extractPoint(wrapperData, ok);
    }

    const QJsonObject wrapperPayload = data.value(QStringLiteral("payload")).toObject();
    if (!wrapperPayload.isEmpty()) {
        return extractPoint(wrapperPayload, ok);
    }

    if (data.contains(QStringLiteral("x")) || data.contains(QStringLiteral("y"))) {
        if (ok) {
            *ok = true;
        }
        return QPointF(data.value(QStringLiteral("x")).toDouble(),
                       data.value(QStringLiteral("y")).toDouble());
    }

    return QPointF();
}

double extractLinearSpeed(const QJsonObject &data)
{
    const QJsonObject velocity = data.value(QStringLiteral("velocity")).toObject();
    if (!velocity.isEmpty()) {
        if (velocity.contains(QStringLiteral("linear_x"))) {
            return std::abs(velocity.value(QStringLiteral("linear_x")).toDouble());
        }
        if (velocity.contains(QStringLiteral("linear"))) {
            return std::abs(velocity.value(QStringLiteral("linear")).toDouble());
        }
        if (velocity.contains(QStringLiteral("x"))) {
            return std::abs(velocity.value(QStringLiteral("x")).toDouble());
        }
    }

    const QJsonObject twist = data.value(QStringLiteral("twist")).toObject();
    if (!twist.isEmpty()) {
        return extractLinearSpeed(twist);
    }

    const QJsonObject linear = data.value(QStringLiteral("linear")).toObject();
    if (!linear.isEmpty() && linear.contains(QStringLiteral("x"))) {
        return std::abs(linear.value(QStringLiteral("x")).toDouble());
    }

    const QJsonObject wrapperMsg = data.value(QStringLiteral("msg")).toObject();
    if (!wrapperMsg.isEmpty()) {
        return extractLinearSpeed(wrapperMsg);
    }

    const QJsonObject wrapperData = data.value(QStringLiteral("data")).toObject();
    if (!wrapperData.isEmpty()) {
        return extractLinearSpeed(wrapperData);
    }

    const QJsonObject wrapperPayload = data.value(QStringLiteral("payload")).toObject();
    if (!wrapperPayload.isEmpty()) {
        return extractLinearSpeed(wrapperPayload);
    }

    if (data.contains(QStringLiteral("speed"))) {
        return std::abs(data.value(QStringLiteral("speed")).toDouble());
    }

    return 0.0;
}

double extractAngularSpeed(const QJsonObject &data)
{
    const QJsonObject velocity = data.value(QStringLiteral("velocity")).toObject();
    if (!velocity.isEmpty()) {
        if (velocity.contains(QStringLiteral("angular_z"))) {
            return std::abs(velocity.value(QStringLiteral("angular_z")).toDouble());
        }
        if (velocity.contains(QStringLiteral("angular"))) {
            return std::abs(velocity.value(QStringLiteral("angular")).toDouble());
        }
        if (velocity.contains(QStringLiteral("z"))) {
            return std::abs(velocity.value(QStringLiteral("z")).toDouble());
        }
    }

    const QJsonObject twist = data.value(QStringLiteral("twist")).toObject();
    if (!twist.isEmpty()) {
        return extractAngularSpeed(twist);
    }

    const QJsonObject angular = data.value(QStringLiteral("angular")).toObject();
    if (!angular.isEmpty() && angular.contains(QStringLiteral("z"))) {
        return std::abs(angular.value(QStringLiteral("z")).toDouble());
    }

    const QJsonObject wrapperMsg = data.value(QStringLiteral("msg")).toObject();
    if (!wrapperMsg.isEmpty()) {
        return extractAngularSpeed(wrapperMsg);
    }

    const QJsonObject wrapperData = data.value(QStringLiteral("data")).toObject();
    if (!wrapperData.isEmpty()) {
        return extractAngularSpeed(wrapperData);
    }

    const QJsonObject wrapperPayload = data.value(QStringLiteral("payload")).toObject();
    if (!wrapperPayload.isEmpty()) {
        return extractAngularSpeed(wrapperPayload);
    }

    return 0.0;
}

bool hasSuccessStatus(const QJsonObject &data)
{
    const QStringList stringKeys = {
        QStringLiteral("status"),
        QStringLiteral("state"),
        QStringLiteral("result"),
        QStringLiteral("outcome"),
        QStringLiteral("goal_status"),
        QStringLiteral("event"),
        QStringLiteral("message")
    };

    for (const QString &key : stringKeys) {
        const QString value = ownedTrimmedString(data.value(key)).toLower();
        if (value.isEmpty()) {
            continue;
        }
        if (value.contains(QStringLiteral("succeed"))
            || value.contains(QStringLiteral("success"))
            || value.contains(QStringLiteral("arriv"))
            || value.contains(QStringLiteral("reach"))
            || value.contains(QStringLiteral("complete"))
            || value.contains(QStringLiteral("done"))) {
            return true;
        }
    }

    const QStringList numericKeys = {
        QStringLiteral("status"),
        QStringLiteral("status_code"),
        QStringLiteral("code"),
        QStringLiteral("result_code")
    };

    for (const QString &key : numericKeys) {
        const QJsonValue value = data.value(key);
        if (!value.isDouble()) {
            continue;
        }
        const int numeric = value.toInt();
        if (numeric == 3 || numeric == 4) {
            return true;
        }
    }

    for (const QString &wrapperKey : {QStringLiteral("msg"), QStringLiteral("data"), QStringLiteral("payload")}) {
        const QJsonObject wrapper = data.value(wrapperKey).toObject();
        if (!wrapper.isEmpty() && hasSuccessStatus(wrapper)) {
            return true;
        }
    }

    return false;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlag(Qt::FramelessWindowHint, true);
    setWindowTitle(QStringLiteral("CCTV Control Center"));
    resize(1280, 720);

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

MainWindow::~MainWindow() = default;

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        const int key = keyEvent->key();

        if (key == Qt::Key_W || key == Qt::Key_S || key == Qt::Key_A || key == Qt::Key_D) {
            QWidget *focusWidget = QApplication::focusWidget();
            if (focusWidget) {
                if (qobject_cast<QLineEdit *>(focusWidget)
                    || qobject_cast<QTextEdit *>(focusWidget)
                    || qobject_cast<QPlainTextEdit *>(focusWidget)) {
                    return false;
                }
            }

            return handleWasdKey(keyEvent, event->type() == QEvent::KeyPress);
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

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    QMainWindow::keyPressEvent(event);
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
    m_rosClient->sendModeControl(isManualMode ? QStringLiteral("manual")
                                              : QStringLiteral("auto"));

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
    const QPointF robotPosition = extractPoint(data, &ok);
    if (!ok) {
        return;
    }

    const double distance = std::hypot(robotPosition.x() - m_activeGoalPosition.x(),
                                       robotPosition.y() - m_activeGoalPosition.y());
    const double linearSpeed = extractLinearSpeed(data);
    const double angularSpeed = extractAngularSpeed(data);
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
    if (!m_hasActiveGoal || !hasSuccessStatus(data)) {
        return;
    }

    qDebug() << "[MainWindow] Goal reached by nav_status:"
             << QJsonDocument(data).toJson(QJsonDocument::Compact);
    clearAllGoalOverlays();
    clearGoalTracking();
}

void MainWindow::handleGoalNavFeedback(const QJsonObject &data)
{
    if (!m_hasActiveGoal || !hasSuccessStatus(data)) {
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
