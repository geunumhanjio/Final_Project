#include "liveview.h"
#include "streammanager.h"
#include "configmanager.h"
#include <QDebug>
#include <QTimer>
#include <QShowEvent>

LiveView::LiveView(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    // Background is handled by global stylesheet or main window, but we can set it explicitly if needed
    
    // Main Layout: Split into Grid (Left 2x2) and Side Panel (Right 1x2)
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    setFocusPolicy(Qt::ClickFocus); // [Fix] Allow clicking background to regain focus

    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // 1. Left Grid (CCTV 2x2)
    QWidget *gridContainer = new QWidget(this);
    gridLayout = new QGridLayout(gridContainer);
    gridLayout->setSpacing(4);
    gridLayout->setContentsMargins(0, 0, 0, 0);

    for(int i = 0; i < 4; i++) {
        cctvWidgets[i] = new VideoCard(this);
        cctvWidgets[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        cctvWidgets[i]->setMinimumSize(320, 240);
        cctvWidgets[i]->setChannelName(QString("Channel %1 - Camera").arg(i+1));
        cctvWidgets[i]->setChannelId(i + 1); // 1-based ID
        
        // Connect internal fullscreen button signal
        connect(cctvWidgets[i], &VideoCard::fullScreenRequested, [=](){
             QString url = "";
             if (i < highQualityUrls.size()) url = highQualityUrls[i];
             emit requestFullScreen(i, url);
        });

        // Connect record signal
        connect(cctvWidgets[i], &VideoCard::recordRequested, this, &LiveView::recordCommandRequested);
        
        gridLayout->addWidget(cctvWidgets[i], i / 2, i % 2);
    }
    
    // 2. Right Panel (RC Car & Lidar)
    rightPanel = new QWidget(this);
    // rightPanel->setFixedWidth(400); // Removed fixed width
    rightPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(4);
    
    // 2-1. RC Car Camera (VideoCard)
    rcCarCamWidget = new VideoCard(this);
    rcCarCamWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    rcCarCamWidget->setMinimumSize(320, 240);
    rcCarCamWidget->setChannelName("RC Car - Front Cam");
    rcCarCamWidget->setChannelId(9); // ID 9 for RC Car (Avoid conflict with High Quality ID 5)

    // Connect internal fullscreen button signal for RC Car
    connect(rcCarCamWidget, &VideoCard::fullScreenRequested, [=](){
            // Use current IP/Port settings to construct URL
            ConfigManager::instance().loadDefaults();
            QString ip = ConfigManager::instance().getCameraIp();
            QString port = ConfigManager::instance().getCameraPort();
            QString url = QString("rtsp://%1:%2/robot_cam").arg(ip, port);
            emit requestFullScreen(4, url); // Index 4 for RC Car
    });
    
    // Connect record signal
    connect(rcCarCamWidget, &VideoCard::recordRequested, this, &LiveView::recordCommandRequested);

    rightLayout->addWidget(rcCarCamWidget);

    // 2-2. SLAM Map (QLabel in Card)
    QWidget *slamCard = new QWidget(rightPanel);
    slamCard->setObjectName("SensorCard");
    // Style moved to QSS
        
    QVBoxLayout *slamLayout = new QVBoxLayout(slamCard);
    slamLayout->setContentsMargins(0, 0, 0, 0);
        
    // Header
    QWidget *slamHeader = new QWidget(slamCard);
    slamHeader->setObjectName("SensorHeader");
    slamHeader->setFixedHeight(30);
        
    QHBoxLayout *headerLayout = new QHBoxLayout(slamHeader);
    headerLayout->setContentsMargins(8, 0, 8, 0);
    QLabel *slamTitle = new QLabel("RC Car - SLAM Lidar Map", slamHeader);
    slamTitle->setObjectName("SensorTitle");
    slamTitle->setStyleSheet("font-weight: bold; font-size: 11px;");
    headerLayout->addWidget(slamTitle);
        
    // Content
    slamMapWidget = new QLabel("RC Car - SLAM Lidar Map", slamCard);
    slamMapWidget->setObjectName("SensorValue");
    slamMapWidget->setAlignment(Qt::AlignCenter);
    slamMapWidget->setStyleSheet("background-color: transparent; font-size: 14px;"); 
    slamMapWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        
    slamLayout->addWidget(slamHeader);
    slamLayout->addWidget(slamMapWidget);
        
    rightLayout->addWidget(slamCard);

    // Add to main layout
    // Grid takes 2/3, Right Panel takes 1/3 -> Equal column widths
    mainLayout->addWidget(gridContainer, 2); 
    mainLayout->addWidget(rightPanel, 1);

    // if (!gst_is_initialized()) gst_init(nullptr, nullptr); // Moved to main.cpp
    streamStarted = false;

    // Connect to StreamManager config change
    connect(&StreamManager::instance(), &StreamManager::configLoaded, this, &LiveView::refreshStreams);
}

void LiveView::refreshStreams()
{
    qDebug() << "[LiveView] Refreshing streams due to config change...";
    stopAll();
    
    // Slight delay to ensure cleanup before restart
    QTimer::singleShot(200, this, [this](){
        initCCTVStreams();
    });
}

void LiveView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!streamStarted) {
        QTimer::singleShot(500, this, [this]() { initCCTVStreams(); });
        streamStarted = true;
    }
}

void LiveView::initCCTVStreams()
{
    qDebug() << "[LiveView] Initializing RTSP connections...";
    streamStarted = true; // [Fix] Ensure flag is set when streams are initialized

    lowQualityUrls.clear();
    highQualityUrls.clear();

    // Use StreamManager
    // StreamManager::instance().loadConfig(); // REMOVED: Triggers endless loop with refreshStreams
    QList<ChannelConfig> channels = StreamManager::instance().getChannels();

    for(const ChannelConfig &ch : channels) {
        lowQualityUrls << ch.urlLow;
        highQualityUrls << ch.urlHigh;
    }

    for(int i = 0; i < 4; i++) {
        if (i >= lowQualityUrls.size()) break;
        QString url = lowQualityUrls[i];
        int delay = i * 100;

        QTimer::singleShot(delay, this, [this, i, url]() {
            if(cctvWidgets[i]) {
                cctvWidgets[i]->playUrl(url, 200);
            }
        });
    }

    // Start RC Car Stream
    QString ip = ConfigManager::instance().getCameraIp();
    // QString port = ConfigManager::instance().getCameraPort();
    // [Mod] Use specific URL for RC Car as requested
    QString rcUrl = QString("rtsp://%1:9554/camera").arg(ip); 
    // If exact IP is required regardless of config: QString rcUrl = "rtsp://192.168.0.237:9554/camera";
    // Assuming IP might match config, but port/path is different. 
    // Let's stick to the requested URL exactly to be safe, or use IP from config if it matches?
    // User said: "rtsp://192.168.0.237:9554/camera 이 주소로..."
    // I will use ConfigManager IP if it matches 192.168.0.237 usually, but to be safe and exact:
    bool useConfigIp = (ip == "192.168.0.237"); // Just logical check, not code
    // I will simply set it to the hardcoded string or usage of IP if they want dynamic.
    // Given the request is specific:
    rcUrl = "rtsp://192.168.0.237:9554/camera";
    
    QTimer::singleShot(400, this, [this, rcUrl]() {
        if(rcCarCamWidget) {
            rcCarCamWidget->playUrl(rcUrl, 200);
        }
    });
}

bool LiveView::eventFilter(QObject *obj, QEvent *event) {
    // Keep event filter if we want to handle other things, 
    // but fullscreen is now handled by VideoCard signal.
    return QWidget::eventFilter(obj, event);
}

// [Fix] Click background to focus (for WASD)
void LiveView::mousePressEvent(QMouseEvent *event) {
    this->setFocus();
    QWidget::mousePressEvent(event);
}

void LiveView::setChannelVisible(int index, bool visible) {
    if (index < 4) {
        cctvWidgets[index]->setVisible(visible);
        updateCCTVLayout(); // Recalculate grid
    }
    else if (index < 6) {
        if (index == 4) { // RC Car Cam
             if(rcCarCamWidget) rcCarCamWidget->setVisible(visible);
        } else if (index == 5) { // SLAM Map
             if(slamMapWidget && slamMapWidget->parentWidget())
                 slamMapWidget->parentWidget()->setVisible(visible);
        }
        
        // Check if both sensors are hidden
        bool anySensorVisible = false;
        if(rcCarCamWidget && !rcCarCamWidget->isHidden()) anySensorVisible = true;
        if(slamMapWidget && slamMapWidget->parentWidget() && !slamMapWidget->parentWidget()->isHidden()) anySensorVisible = true;

        rightPanel->setVisible(anySensorVisible);
    }
}

void LiveView::updateCCTVLayout()
{
    // 1. Identify visibility state
    bool v[4];
    for(int i=0; i<4; i++) v[i] = (cctvWidgets[i] && !cctvWidgets[i]->isHidden());

    // 2. Clear grid layout
    QLayoutItem *item;
    while ((item = gridLayout->takeAt(0)) != nullptr) {
        delete item; 
    }

    // 3. Place Widgets
    // Special Case: Diagonal 1 & 4 only -> Force Top/Bottom Split
    if (v[0] && v[3] && !v[1] && !v[2]) {
        gridLayout->addWidget(cctvWidgets[0], 0, 0, 1, 2); // Row 0 Full
        gridLayout->addWidget(cctvWidgets[3], 1, 0, 1, 2); // Row 1 Full
    }
    // Special Case: Diagonal 2 & 3 only -> Force Top/Bottom Split
    else if (v[1] && v[2] && !v[0] && !v[3]) {
        gridLayout->addWidget(cctvWidgets[1], 0, 0, 1, 2); // Row 0 Full
        gridLayout->addWidget(cctvWidgets[2], 1, 0, 1, 2); // Row 1 Full
    }
    else {
        // Standard Fixed Positioning (Preserve Original Slots)
        if(v[0]) gridLayout->addWidget(cctvWidgets[0], 0, 0);
        if(v[1]) gridLayout->addWidget(cctvWidgets[1], 0, 1);
        if(v[2]) gridLayout->addWidget(cctvWidgets[2], 1, 0);
        if(v[3]) gridLayout->addWidget(cctvWidgets[3], 1, 1);
    }
    
    // 4. Update Stretch Factors (Smart Collapse)
    // If a row/col is completely empty, set stretch to 0 so others expand.
    bool row0HasItems = v[0] || v[1];
    bool row1HasItems = v[2] || v[3];
    bool col0HasItems = v[0] || v[2];
    bool col1HasItems = v[1] || v[3];

    // Check diagonal special cases override (they occupy all cols)
    if ((v[0] && v[3] && !v[1] && !v[2]) || (v[1] && v[2] && !v[0] && !v[3])) {
        col0HasItems = true; col1HasItems = true; 
    }

    gridLayout->setRowStretch(0, row0HasItems ? 1 : 0);
    gridLayout->setRowStretch(1, row1HasItems ? 1 : 0);
    gridLayout->setColumnStretch(0, col0HasItems ? 1 : 0);
    gridLayout->setColumnStretch(1, col1HasItems ? 1 : 0);
}

void LiveView::stopAll() {
    for(int i = 0; i < 4; i++) if (cctvWidgets[i]) cctvWidgets[i]->stop();
    if (rcCarCamWidget) rcCarCamWidget->stop();
    streamStarted = false; // [Fix] Reset flag so showEvent restarts streams
}
