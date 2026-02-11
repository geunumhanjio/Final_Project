#include "mainwindow.h"
#include "fullscreenview.h"
#include "settingswidget.h"
#include <QCloseEvent>
#include <QApplication>
#include <QFile>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    this->setWindowTitle("CCTV 통합 관제 시스템 - 근엄한조");
    this->resize(1280, 720);
    m_isDark = true; // Default to dark
    initUI();
    initConnections();
    
    
    // Load initial theme
    loadTheme("style/theme_dark.qss");
    
    // ROS2 Control Init
    m_rosClient = new RosBridgeClient(this);
    m_rosClient->connectToHost("ws://192.168.0.237:9090");
    
    m_inputTimer = new QTimer(this);
    connect(m_inputTimer, &QTimer::timeout, this, &MainWindow::processInput);
}

MainWindow::~MainWindow() { }

void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (event->isAutoRepeat()) return; // Ignore auto-repeat key presses
    
    int key = event->key();
    // [Mod] WASD Control
    if (key == Qt::Key_W || key == Qt::Key_S || key == Qt::Key_A || key == Qt::Key_D) {
        if (!m_pressedKeys.contains(key)) {
            m_pressedKeys.insert(key);
            if (!m_inputTimer->isActive()) {
                m_inputTimer->start(100); // 10Hz
                processInput(); // Immediate response
            }
        }
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *event) {
    if (event->isAutoRepeat()) return;

    int key = event->key();
    if (m_pressedKeys.contains(key)) {
        m_pressedKeys.remove(key);
        
        if (m_pressedKeys.isEmpty()) {
            m_inputTimer->stop();
            // Send Stop Command
            m_rosClient->sendCmdVel(0.0, 0.0);
            qDebug() << "[Control] STOP";
        } else {
            processInput(); // Update direction immediately
        }
    } else {
        QMainWindow::keyReleaseEvent(event);
    }
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
    m_centralStack = new QStackedWidget(this);
    this->setCentralWidget(m_centralStack);

    m_livePage = new LiveView(this);
    m_playbackPage = new QLabel("기록 화면", this);
    m_settingsPage = new SettingsWidget(this);
    m_fullPage = new FullScreenView(this);

    m_playbackPage->setStyleSheet("color:white; font-size:20px;");
    m_settingsPage->setStyleSheet("color:white; font-size:20px;");
    m_playbackPage->setAlignment(Qt::AlignCenter);
    m_playbackPage->setAlignment(Qt::AlignCenter);
    // m_settingsPage->setAlignment(Qt::AlignCenter); // SettingsWidget is not a QLabel

    m_centralStack->addWidget(m_livePage);
    m_centralStack->addWidget(m_playbackPage);
    m_centralStack->addWidget(m_settingsPage);
    m_centralStack->addWidget(m_fullPage);
}

void MainWindow::initConnections()
{
    connect(m_topBar, &TopBar::sidebarToggled, [=](){ m_sidebar->setVisible(!m_sidebar->isVisible()); });
    connect(m_topBar, &TopBar::modeChanged, m_centralStack, &QStackedWidget::setCurrentIndex);
    connect(m_topBar, &TopBar::themeToggled, this, &MainWindow::toggleTheme);
    connect(m_sidebar, &Sidebar::channelStateChanged, m_livePage, &LiveView::setChannelVisible);

    connect(m_livePage, &LiveView::requestFullScreen, [=](int index, QString url){
        if (index <= 4 && !url.isEmpty()) {
            m_fullPage->play(url, index);
            m_centralStack->setCurrentWidget(m_fullPage);
        } else {
            m_centralStack->setCurrentWidget(m_fullPage);
        }
    });

    connect(m_fullPage, &FullScreenView::closeRequested, [=](){
        m_fullPage->stop();
        m_centralStack->setCurrentWidget(m_livePage);
    });
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
    if(m_fullPage) m_fullPage->updateTheme(m_isDark);
    if(m_topBar) m_topBar->updateTheme(m_isDark);
}
