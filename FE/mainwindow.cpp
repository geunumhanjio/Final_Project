#include "mainwindow.h"
#include "fullscreenview.h"
#include "settingswidget.h"
#include "playbackview.h" // [New]
#include <QCloseEvent>
#include <QApplication>
#include <QFile>
#include <QDir>
#include <QDir>
#include <QStandardPaths> 
#include <QLineEdit> // [New]
#include <QTextEdit> // [New]
#include <QPlainTextEdit> // [New]
#include <QDialog>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QColor>
#include <QGraphicsDropShadowEffect>
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

    const QJsonObject position = data.value("position").toObject();
    if (!position.isEmpty()) {
        if (ok) {
            *ok = position.contains("x") || position.contains("y");
        }
        return QPointF(position.value("x").toDouble(), position.value("y").toDouble());
    }

    const QJsonObject translation = data.value("translation").toObject();
    if (!translation.isEmpty()) {
        if (ok) {
            *ok = translation.contains("x") || translation.contains("y");
        }
        return QPointF(translation.value("x").toDouble(), translation.value("y").toDouble());
    }

    const QJsonObject pose = data.value("pose").toObject();
    if (!pose.isEmpty()) {
        return extractPoint(pose, ok);
    }

    const QJsonObject transform = data.value("transform").toObject();
    if (!transform.isEmpty()) {
        return extractPoint(transform, ok);
    }

    const QJsonObject wrapperMsg = data.value("msg").toObject();
    if (!wrapperMsg.isEmpty()) {
        return extractPoint(wrapperMsg, ok);
    }

    const QJsonObject wrapperData = data.value("data").toObject();
    if (!wrapperData.isEmpty()) {
        return extractPoint(wrapperData, ok);
    }

    const QJsonObject wrapperPayload = data.value("payload").toObject();
    if (!wrapperPayload.isEmpty()) {
        return extractPoint(wrapperPayload, ok);
    }

    if (data.contains("x") || data.contains("y")) {
        if (ok) {
            *ok = true;
        }
        return QPointF(data.value("x").toDouble(), data.value("y").toDouble());
    }

    return QPointF();
}

double extractLinearSpeed(const QJsonObject &data)
{
    const QJsonObject velocity = data.value("velocity").toObject();
    if (!velocity.isEmpty()) {
        if (velocity.contains("linear_x")) {
            return std::abs(velocity.value("linear_x").toDouble());
        }
        if (velocity.contains("linear")) {
            return std::abs(velocity.value("linear").toDouble());
        }
        if (velocity.contains("x")) {
            return std::abs(velocity.value("x").toDouble());
        }
    }

    const QJsonObject twist = data.value("twist").toObject();
    if (!twist.isEmpty()) {
        return extractLinearSpeed(twist);
    }

    const QJsonObject linear = data.value("linear").toObject();
    if (!linear.isEmpty()) {
        if (linear.contains("x")) {
            return std::abs(linear.value("x").toDouble());
        }
    }

    const QJsonObject wrapperMsg = data.value("msg").toObject();
    if (!wrapperMsg.isEmpty()) {
        return extractLinearSpeed(wrapperMsg);
    }

    const QJsonObject wrapperData = data.value("data").toObject();
    if (!wrapperData.isEmpty()) {
        return extractLinearSpeed(wrapperData);
    }

    const QJsonObject wrapperPayload = data.value("payload").toObject();
    if (!wrapperPayload.isEmpty()) {
        return extractLinearSpeed(wrapperPayload);
    }

    if (data.contains("speed")) {
        return std::abs(data.value("speed").toDouble());
    }

    return 0.0;
}

double extractAngularSpeed(const QJsonObject &data)
{
    const QJsonObject velocity = data.value("velocity").toObject();
    if (!velocity.isEmpty()) {
        if (velocity.contains("angular_z")) {
            return std::abs(velocity.value("angular_z").toDouble());
        }
        if (velocity.contains("angular")) {
            return std::abs(velocity.value("angular").toDouble());
        }
        if (velocity.contains("z")) {
            return std::abs(velocity.value("z").toDouble());
        }
    }

    const QJsonObject twist = data.value("twist").toObject();
    if (!twist.isEmpty()) {
        return extractAngularSpeed(twist);
    }

    const QJsonObject angular = data.value("angular").toObject();
    if (!angular.isEmpty()) {
        if (angular.contains("z")) {
            return std::abs(angular.value("z").toDouble());
        }
    }

    const QJsonObject wrapperMsg = data.value("msg").toObject();
    if (!wrapperMsg.isEmpty()) {
        return extractAngularSpeed(wrapperMsg);
    }

    const QJsonObject wrapperData = data.value("data").toObject();
    if (!wrapperData.isEmpty()) {
        return extractAngularSpeed(wrapperData);
    }

    const QJsonObject wrapperPayload = data.value("payload").toObject();
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
        if (value.contains("succeed") || value.contains("success")
            || value.contains("arriv") || value.contains("reach")
            || value.contains("complete") || value.contains("done")) {
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

bool showFramelessCloseDialog(QWidget *parent, bool darkTheme)
{
    QDialog dialog(parent, Qt::Dialog | Qt::FramelessWindowHint | Qt::CustomizeWindowHint);
    dialog.setModal(true);
    dialog.setAttribute(Qt::WA_TranslucentBackground);

    auto *rootLayout = new QVBoxLayout(&dialog);
    rootLayout->setContentsMargins(20, 20, 20, 20);

    auto *panel = new QFrame(&dialog);
    panel->setObjectName("CloseConfirmPanel");
    panel->setMinimumWidth(360);

    const QString panelStyle = darkTheme
        ? QStringLiteral(
            "QFrame#CloseConfirmPanel {"
            " background-color: #182131;"
            " border: 1px solid #2a3649;"
            " border-radius: 18px;"
            "}"
            "QLabel#CloseConfirmTitle {"
            " color: #f8fafc;"
            " font-size: 20px;"
            " font-weight: 700;"
            " background: transparent;"
            "}"
            "QLabel#CloseConfirmText {"
            " color: #cbd5e1;"
            " font-size: 14px;"
            " background: transparent;"
            "}"
            "QLabel#CloseConfirmIcon {"
            " background: rgba(239, 68, 68, 0.16);"
            " color: #f87171;"
            " border: 1px solid rgba(248, 113, 113, 0.28);"
            " border-radius: 22px;"
            " font-size: 18px;"
            " font-weight: 700;"
            "}"
            "QPushButton#CloseConfirmCancel {"
            " background-color: #243041;"
            " color: #e2e8f0;"
            " border: 1px solid #334155;"
            " border-radius: 10px;"
            " padding: 10px 18px;"
            " min-width: 88px;"
            "}"
            "QPushButton#CloseConfirmCancel:hover { background-color: #2d3b4f; }"
            "QPushButton#CloseConfirmAccept {"
            " background-color: #dc2626;"
            " color: white;"
            " border: none;"
            " border-radius: 10px;"
            " padding: 10px 18px;"
            " min-width: 88px;"
            "}"
            "QPushButton#CloseConfirmAccept:hover { background-color: #b91c1c; }")
        : QStringLiteral(
            "QFrame#CloseConfirmPanel {"
            " background-color: #ffffff;"
            " border: 1px solid #e2e8f0;"
            " border-radius: 18px;"
            "}"
            "QLabel#CloseConfirmTitle {"
            " color: #0f172a;"
            " font-size: 20px;"
            " font-weight: 700;"
            " background: transparent;"
            "}"
            "QLabel#CloseConfirmText {"
            " color: #475569;"
            " font-size: 14px;"
            " background: transparent;"
            "}"
            "QLabel#CloseConfirmIcon {"
            " background: #fff1f2;"
            " color: #dc2626;"
            " border: 1px solid #fecdd3;"
            " border-radius: 22px;"
            " font-size: 18px;"
            " font-weight: 700;"
            "}"
            "QPushButton#CloseConfirmCancel {"
            " background-color: #f8fafc;"
            " color: #0f172a;"
            " border: 1px solid #cbd5e1;"
            " border-radius: 10px;"
            " padding: 10px 18px;"
            " min-width: 88px;"
            "}"
            "QPushButton#CloseConfirmCancel:hover { background-color: #f1f5f9; }"
            "QPushButton#CloseConfirmAccept {"
            " background-color: #ef4444;"
            " color: white;"
            " border: none;"
            " border-radius: 10px;"
            " padding: 10px 18px;"
            " min-width: 88px;"
            "}"
            "QPushButton#CloseConfirmAccept:hover { background-color: #dc2626; }");
    panel->setStyleSheet(panelStyle);

    auto *shadow = new QGraphicsDropShadowEffect(panel);
    shadow->setBlurRadius(36);
    shadow->setOffset(0, 14);
    shadow->setColor(darkTheme ? QColor(0, 0, 0, 150) : QColor(15, 23, 42, 45));
    panel->setGraphicsEffect(shadow);

    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(24, 22, 24, 20);
    panelLayout->setSpacing(18);

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(14);

    auto *iconLabel = new QLabel(QStringLiteral("X"), panel);
    iconLabel->setObjectName("CloseConfirmIcon");
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setFixedSize(44, 44);

    auto *titleWrap = new QVBoxLayout();
    titleWrap->setSpacing(4);
    titleWrap->setContentsMargins(0, 0, 0, 0);

    auto *titleLabel = new QLabel(QStringLiteral("\uC571 \uC885\uB8CC"), panel);
    titleLabel->setObjectName("CloseConfirmTitle");

    auto *messageLabel = new QLabel(QStringLiteral("\uC885\uB8CC\uD558\uC2DC\uACA0\uC2B5\uB2C8\uAE4C?"), panel);
    messageLabel->setObjectName("CloseConfirmText");
    messageLabel->setWordWrap(true);

    titleWrap->addWidget(titleLabel);
    titleWrap->addWidget(messageLabel);

    headerLayout->addWidget(iconLabel, 0, Qt::AlignTop);
    headerLayout->addLayout(titleWrap, 1);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    buttonLayout->addStretch();

    auto *cancelButton = new QPushButton(QStringLiteral("\uC544\uB2C8\uC694"), panel);
    cancelButton->setObjectName("CloseConfirmCancel");
    cancelButton->setCursor(Qt::PointingHandCursor);
    cancelButton->setDefault(true);

    auto *acceptButton = new QPushButton(QStringLiteral("\uC608"), panel);
    acceptButton->setObjectName("CloseConfirmAccept");
    acceptButton->setCursor(Qt::PointingHandCursor);

    QObject::connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(acceptButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(acceptButton);

    panelLayout->addLayout(headerLayout);
    panelLayout->addLayout(buttonLayout);
    rootLayout->addWidget(panel);

    dialog.adjustSize();
    if (parent) {
        dialog.move(parent->frameGeometry().center() - dialog.rect().center());
    }

    return dialog.exec() == QDialog::Accepted;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlag(Qt::FramelessWindowHint, true);
    this->setWindowTitle("CCTV 통합 관제 시스템 - 근엄한조");
    this->resize(1280, 720);
    m_isDark = true; // Default to dark
    
    // [New] Install global event filter for WASD
    qApp->installEventFilter(this);

    // [Fix] Initialize Clients BEFORE initConnections
    qDebug() << "[MainWindow] Initializing RosBridgeClient...";
    ConfigManager::instance().loadDefaults();
    m_rosClient = new RosBridgeClient(this);
    m_currentRobotWsUrl = ConfigManager::instance().getRobotIp();
    m_rosClient->connectToHost(m_currentRobotWsUrl);
    
    qDebug() << "[MainWindow] Initializing CameraControlClient...";
    m_cameraClient = new CameraControlClient(this);

    initUI();
    initConnections();
    
    // Load initial theme
    loadTheme("style/theme_dark.qss");
    
    m_inputTimer = new QTimer(this);
    connect(m_inputTimer, &QTimer::timeout, this, &MainWindow::processInput);
    
    // Listen to config changes
    connect(&ConfigManager::instance(), &ConfigManager::configChanged, this, &MainWindow::onConfigChanged);
}

MainWindow::~MainWindow() { }

// [New] Global Event Filter for WASD
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        int key = ke->key();

        // Check for WASD keys
        if (key == Qt::Key_W || key == Qt::Key_S || key == Qt::Key_A || key == Qt::Key_D) {
            
            // [Check] Is user typing in a text field?
            QWidget *focusW = QApplication::focusWidget();
            if (focusW) {
                if (qobject_cast<QLineEdit*>(focusW) || qobject_cast<QTextEdit*>(focusW) || qobject_cast<QPlainTextEdit*>(focusW)) {
                    // Let the text widget handle it
                    return false; 
                }
            }

            // Handle Robot Control
            return handleWasdKey(ke, event->type() == QEvent::KeyPress);
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// [New] Shared WASD Logic
bool MainWindow::handleWasdKey(QKeyEvent *event, bool isPress) {
    if (m_robotMode != Sidebar::ManualMode) return false;
    if (!ConfigManager::instance().getManualControl()) return false;
    if (event->isAutoRepeat()) return false;
    
    int key = event->key();

    if (isPress) {
        if (!m_pressedKeys.contains(key)) {
            m_pressedKeys.insert(key);
            if (!m_inputTimer->isActive()) {
                m_inputTimer->start(100); 
                processInput(); 
            }
        }
    } else {
        if (m_pressedKeys.contains(key)) {
            m_pressedKeys.remove(key);
            if (m_pressedKeys.isEmpty()) {
                m_inputTimer->stop();
                m_rosClient->sendCmdVel(0.0, 0.0);
                qDebug() << "[Control] STOP";
            } else {
                processInput();
            }
        }
    }
    return true; // Consume event
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    // Other keys usually handled by shortcuts or specific widgets.
    // WASD is now handled by eventFilter.
    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event) {
    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::processInput() {
    double linear = 0.0;
    double angular = 0.0;
    const double linearStep = ConfigManager::instance().getManualLinearX();
    const double angularStep = ConfigManager::instance().getManualAngularZ();
    
    if (m_pressedKeys.contains(Qt::Key_W)) linear += linearStep;
    if (m_pressedKeys.contains(Qt::Key_S)) linear -= linearStep;
    if (m_pressedKeys.contains(Qt::Key_A)) angular += angularStep;
    if (m_pressedKeys.contains(Qt::Key_D)) angular -= angularStep;
    
    m_rosClient->sendCmdVel(linear, angular);
    // qDebug() << "[Control] Linear:" << linear << "Angular:" << angular;
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

    qDebug() << "[MainWindow] Goal reached by nav_status:" << QJsonDocument(data).toJson(QJsonDocument::Compact);
    clearAllGoalOverlays();
    clearGoalTracking();
}

void MainWindow::handleGoalNavFeedback(const QJsonObject &data)
{
    if (!m_hasActiveGoal || !hasSuccessStatus(data)) {
        return;
    }

    qDebug() << "[MainWindow] Goal reached by nav_feedback:" << QJsonDocument(data).toJson(QJsonDocument::Compact);
    clearAllGoalOverlays();
    clearGoalTracking();
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

void MainWindow::setSharedVideoGoalOverlay(int channelIndex, const QPointF &normalizedStart, const QPointF &normalizedEnd)
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
    m_fullPage->setControlModeAvailable(m_robotMode == Sidebar::ControlMode);
}

void MainWindow::loadTheme(const QString &relativePath)
{
    // Use application directory to ensure correct path
    QString appDir = QCoreApplication::applicationDirPath();
    QString fullPath = QDir(appDir).filePath(relativePath);
    
    // Fallback: Check if we are in build dir and styles are in source
    if (!QFile::exists(fullPath)) {
        // Try going up one level (common in build dirs)
        fullPath = QDir(appDir).filePath("../" + relativePath);
        if (!QFile::exists(fullPath)) {
             // Try absolute path if known (Debug fallback)
             fullPath = "D:/work/QT_prac/VEDA_QT_1/FE/" + relativePath; 
        }
    }

    QFile file(fullPath);
    if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString styleSheet = QLatin1String(file.readAll());
        qApp->setStyleSheet(styleSheet);
        qDebug() << "Theme loaded from:" << fullPath;
        file.close();
    } else {
        qDebug() << "FAILED to load theme from:" << fullPath;
    }
}

void MainWindow::initUI()
{
    // this->setStyleSheet("QMainWindow { background-color: #222222; }"); // Handled by QSS now
    m_topBar = new TopBar(this);
    this->setMenuWidget(m_topBar);
    m_sidebar = new Sidebar("채널 목록", this);
    this->addDockWidget(Qt::LeftDockWidgetArea, m_sidebar);
    
    qDebug() << "[MainWindow] Creating Central Stack...";
    m_centralStack = new QStackedWidget(this);
    m_centralStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    this->setCentralWidget(m_centralStack);

    qDebug() << "[MainWindow] Creating LiveView...";
    m_livePage = new LiveView(this);
    
    qDebug() << "[MainWindow] Creating PlaybackView...";
    m_playbackPage = new PlaybackView(this); // [Modified]
    
    qDebug() << "[MainWindow] Creating SettingsWidget...";
    m_settingsPage = new SettingsWidget(this);
    
    qDebug() << "[MainWindow] Creating FullScreenView...";
    m_fullPage = new FullScreenView(this);
    
    qDebug() << "[MainWindow] Adding widgets to stack...";

    m_livePage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    m_playbackPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    m_settingsPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    m_fullPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);

    // m_playbackPage styling removed as it is now VideoWidget
    // m_settingsPage->setAlignment(Qt::AlignCenter); // SettingsWidget is not a QLabel

    m_centralStack->addWidget(m_livePage);
    m_centralStack->addWidget(m_playbackPage);
    m_centralStack->addWidget(m_settingsPage);
    m_centralStack->addWidget(m_fullPage);
    qDebug() << "[MainWindow] initUI Completed.";
}

void MainWindow::initConnections()
{
    qDebug() << "[MainWindow] initConnections Started...";
    
    connect(m_topBar, &TopBar::sidebarToggled, [this](){
        if (!m_sidebar || !m_centralStack || m_centralStack->currentWidget() == m_fullPage) {
            return;
        }
        m_sidebar->setVisible(!m_sidebar->isVisible());
    });
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
    connect(m_topBar, &TopBar::closeRequested, this, &QWidget::close);
    connect(m_centralStack, &QStackedWidget::currentChanged, this, [this](int) {
        updateSidebarForCurrentPage();
        applySharedVideoGoalOverlay();
    });
    connect(m_sidebar, &Sidebar::channelStateChanged, m_livePage, &LiveView::setChannelVisible);
    connect(m_sidebar, &Sidebar::robotModeChanged, this, &MainWindow::applyRobotMode);
    connect(m_sidebar, &Sidebar::emergencyStopRequested, this, &MainWindow::requestEmergencyStop);
    connect(m_sidebar, &Sidebar::controlButtonToggled, this, [this](bool enabled) {
        const bool active = enabled && (m_robotMode == Sidebar::ControlMode);
        if (active && m_rosClient) {
            m_rosClient->sendModeControl("auto");
        }
        m_livePage->setGoalTargetingEnabled(active);
        m_fullPage->setControlModeAvailable(m_robotMode == Sidebar::ControlMode);
        m_fullPage->setControlModeChecked(active);
    });
    connect(m_fullPage, &FullScreenView::controlModeRequested, this, [this](bool enabled) {
        if (!m_sidebar) {
            return;
        }

        if (enabled) {
            m_sidebar->setRobotMode(Sidebar::ControlMode);
        } else {
            deactivateControlSession();
        }
    });
    connect(m_livePage, &LiveView::goalInteractionStarted, this, &MainWindow::clearAllGoalOverlays);
    connect(m_livePage, &LiveView::videoGoalOverlayCommitted, this, &MainWindow::setSharedVideoGoalOverlay);
    connect(m_livePage, &LiveView::goalCommitted, this, &MainWindow::deactivateControlSession);
    connect(m_fullPage, &FullScreenView::goalInteractionStarted, this, &MainWindow::clearAllGoalOverlays);
    connect(m_fullPage, &FullScreenView::videoGoalOverlayCommitted, this, &MainWindow::setSharedVideoGoalOverlay);
    connect(m_fullPage, &FullScreenView::videoGoalOverlayClearRequested, this, &MainWindow::clearSharedVideoGoalOverlay);
    connect(m_fullPage, &FullScreenView::goalCommitted, this, &MainWindow::deactivateControlSession);
    connect(m_rosClient, &RosBridgeClient::mapReceived, m_livePage, &LiveView::updateMap);
    connect(m_rosClient, &RosBridgeClient::mapReceived, this, [this](const QJsonObject &data) {
        const QJsonObject info = data.value("info").toObject();
        const QJsonObject origin = info.value("origin").toObject();
        m_fullPage->setMapGeometry(QPointF(origin.value("x").toDouble(), origin.value("y").toDouble()),
                                   info.value("width").toInt(),
                                   info.value("height").toInt(),
                                   info.value("resolution").toDouble());
    });
    connect(m_rosClient, &RosBridgeClient::odomReceived, m_livePage, &LiveView::updateOdom);
    connect(m_rosClient, &RosBridgeClient::odomReceived, this, &MainWindow::handleGoalOdomUpdate);
    connect(m_rosClient, &RosBridgeClient::pathReceived, m_livePage, &LiveView::updatePath);
    connect(m_rosClient, &RosBridgeClient::navStatusReceived, this, &MainWindow::handleGoalNavStatus);
    connect(m_rosClient, &RosBridgeClient::navFeedbackReceived, this, &MainWindow::handleGoalNavFeedback);
    
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
                applySharedVideoGoalOverlay();
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

    } else {
        qCritical() << "[MainWindow] m_cameraClient is NULL in initConnections!";
    }

    connect(m_livePage, &LiveView::requestFullScreen, [=](int index, QString url){
        if (index <= 4 && !url.isEmpty()) {
            qDebug() << "Full Screen Request:" << url;

            m_fullPage->setControlModeAvailable(m_robotMode == Sidebar::ControlMode);
            m_returnToWidget = m_livePage; // [Mod] Return to LiveView on close
            m_centralStack->setCurrentWidget(m_fullPage);
            m_fullPage->play(url, index);
            m_fullPage->setControlModeChecked((m_robotMode == Sidebar::ControlMode) && m_sidebar->isControlButtonActive());
            applySharedVideoGoalOverlay();
        }
    });

    connect(m_fullPage, &FullScreenView::closeRequested, this, [this](){
        if (m_isClosingFullScreen) {
            return;
        }

        m_isClosingFullScreen = true;

        QTimer::singleShot(0, this, [this]() {
            if (m_centralStack) {
                if (m_returnToWidget) {
                    m_centralStack->setCurrentWidget(m_returnToWidget);
                } else if (m_livePage) {
                    m_centralStack->setCurrentWidget(m_livePage);
                }
            }

            updateSidebarForCurrentPage();
            m_isClosingFullScreen = false;
        });
    });
    
    // [New] Connect FullScreenView Recording
    connect(m_fullPage, &FullScreenView::recordRequested, [=](int index, bool start){
        // Map Native Index (0-3 for CCTV, 4 for RC Car) to High Quality IDs
        // CCTV: 0->5, 1->6, 2->7, 3->8
        // RC Car: 4->9
        
        int actualChannelId;
        if (index < 4) {
            actualChannelId = index + 5;
        } else {
            actualChannelId = 9;
        }
        
        ConfigManager::instance().loadDefaults();
        QString ip = ConfigManager::instance().getCameraIp();
        
        // Use m_cameraClient directly
        if (m_cameraClient) {
            m_cameraClient->sendRecordCommand(ip, actualChannelId, start);
             if (start) qDebug() << "[FullScreen] Recording Started on Channel" << actualChannelId;
             else qDebug() << "[FullScreen] Recording Stopped on Channel" << actualChannelId;
        }
    });

    // [New] Connect Goal Pose
    connect(m_fullPage, &FullScreenView::reqGoalPose, [=](double x, double y, double theta){
        if (m_rosClient && m_robotMode == Sidebar::ControlMode) {
            armGoalTracking(QPointF(x, y), theta);
            m_rosClient->sendModeControl("auto");
            m_rosClient->sendGoalPose(x, y, theta);
            deactivateControlSession();
        }
    });
    connect(m_livePage, &LiveView::goalPoseRequested, [=](double x, double y, double theta){
        if (m_rosClient && m_robotMode == Sidebar::ControlMode) {
            armGoalTracking(QPointF(x, y), theta);
            m_rosClient->sendModeControl("auto");
            m_rosClient->sendGoalPose(x, y, theta);
            deactivateControlSession();
        }
    });

    qDebug() << "[MainWindow] initConnections Completed.";
    applyRobotMode(m_sidebar->currentRobotMode());
    updateSidebarForCurrentPage();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!showFramelessCloseDialog(this, m_isDark)) {
        event->ignore();
        return;
    }
#if 0
    messageBox.setIcon(QMessageBox::Question);
    messageBox.setText(QStringLiteral("종료하시겠습니까?"));
    messageBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    messageBox.setDefaultButton(QMessageBox::No);
    messageBox.setWindowModality(Qt::ApplicationModal);
    messageBox.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    messageBox.setStyleSheet(
        "QMessageBox { border: 1px solid #334155; border-radius: 14px; }"
        "QLabel { min-width: 220px; }"
        "QPushButton { min-width: 84px; min-height: 34px; padding: 0 14px; border-radius: 8px; }");

    if (QAbstractButton *yesButton = messageBox.button(QMessageBox::Yes)) {
        yesButton->setText(QStringLiteral("예"));
    }
    if (QAbstractButton *noButton = messageBox.button(QMessageBox::No)) {
        noButton->setText(QStringLiteral("아니요"));
    }

    messageBox.adjustSize();
    messageBox.move(frameGeometry().center() - messageBox.rect().center());

    const QMessageBox::StandardButton reply =
        static_cast<QMessageBox::StandardButton>(messageBox.exec());

    if (reply != QMessageBox::Yes) {
        event->ignore();
        return;
    }

    #endif
    if (m_livePage) {
        m_livePage->stopAll();
    }
    if (m_fullPage) {
        m_fullPage->stop();
    }
    event->accept();
}

void MainWindow::toggleTheme()
{
    m_isDark = !m_isDark;
    QString qssPath = m_isDark ? "style/theme_dark.qss" : "style/theme_light.qss";
    loadTheme(qssPath);
    // [Mod] Removed manual updateTheme calls
}

void MainWindow::onConfigChanged() {
    QString newIp = ConfigManager::instance().getRobotIp();
    if (newIp == m_currentRobotWsUrl) {
        return;
    }

    // Reconnect ROS2 Client only if the target host changed
    qDebug() << "[MainWindow] Config changed. Updating Robot IP to:" << newIp;
    m_currentRobotWsUrl = newIp;
    m_rosClient->disconnect();
    m_rosClient->connectToHost(newIp);
}
