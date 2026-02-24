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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    this->setWindowTitle("CCTV 통합 관제 시스템 - 근엄한조");
    this->resize(1280, 720);
    m_isDark = true; // Default to dark
    
    // [New] Install global event filter for WASD
    qApp->installEventFilter(this);

    // [Fix] Initialize Clients BEFORE initConnections
    qDebug() << "[MainWindow] Initializing RosBridgeClient...";
    m_rosClient = new RosBridgeClient(this);
    m_rosClient->connectToHost(ConfigManager::instance().getRobotIp()); // [Modified] Use config
    
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
    
    if (m_pressedKeys.contains(Qt::Key_W)) linear += 0.3;
    if (m_pressedKeys.contains(Qt::Key_S)) linear -= 0.3;
    if (m_pressedKeys.contains(Qt::Key_A)) angular += 0.5;
    if (m_pressedKeys.contains(Qt::Key_D)) angular -= 0.5;
    
    m_rosClient->sendCmdVel(linear, angular);
    // qDebug() << "[Control] Linear:" << linear << "Angular:" << angular;
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

    m_settingsPage->setStyleSheet("color:white; font-size:20px;");
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
    
    // [New] Connect STREAM_STATS from WebSocket to UI components
    if (m_cameraClient) {
        connect(m_cameraClient, &CameraControlClient::streamStatsReceived, this, [=](int channelId, double fps, double bitrateKbps, double proxyLatencyMs){
            if (m_livePage) m_livePage->updateStreamStats(channelId, fps, bitrateKbps, proxyLatencyMs);
            if (m_fullPage) m_fullPage->updateStreamStats(channelId, fps, bitrateKbps, proxyLatencyMs);
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

    } else {
        qCritical() << "[MainWindow] m_cameraClient is NULL in initConnections!";
    }

    connect(m_livePage, &LiveView::requestFullScreen, [=](int index, QString url){
        if (index <= 4 && !url.isEmpty()) {
            qDebug() << "Full Screen Request:" << url;
            
            m_returnToWidget = m_livePage; // [Mod] Return to LiveView on close
            m_centralStack->setCurrentWidget(m_fullPage);
            m_fullPage->play(url, index);
        }
    });

    connect(m_fullPage, &FullScreenView::closeRequested, [=](){
        m_fullPage->stop();
        // [Mod] Return to previous widget (Live or Playback)
        if (m_returnToWidget) {
            m_centralStack->setCurrentWidget(m_returnToWidget);
        } else {
            m_centralStack->setCurrentWidget(m_livePage); // Default check
        }
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
        if (m_rosClient) {
            qDebug() << "[MainWindow] Sending Goal Pose -> x:" << x << "y:" << y << "theta:" << theta;
            m_rosClient->sendGoalPose(x, y, theta);
        }
    });

    qDebug() << "[MainWindow] initConnections Completed.";
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_livePage) m_livePage->stopAll(); 
    if (m_fullPage) m_fullPage->stop();
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
    // Reconnect ROS2 Client if IP Changed
    qDebug() << "[MainWindow] Config changed. Updating Robot IP to:" << newIp;
    m_rosClient->disconnect();
    m_rosClient->connectToHost(newIp);
}
