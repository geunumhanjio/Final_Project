#include "liveview.h"
#include <QDebug>
#include <QTimer>
#include <QShowEvent>

LiveView::LiveView(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    // Background is handled by global stylesheet or main window, but we can set it explicitly if needed
    
    // Main Layout: Split into Grid (Left 2x2) and Side Panel (Right 1x2)
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
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
        
        // Connect internal fullscreen button signal
        connect(cctvWidgets[i], &VideoCard::fullScreenRequested, [=](){
             QString url = "";
             if (i < highQualityUrls.size()) url = highQualityUrls[i];
             emit requestFullScreen(i, url);
        });
        
        gridLayout->addWidget(cctvWidgets[i], i / 2, i % 2);
    }
    
    // 2. Right Panel (RC Car & Lidar)
    rightPanel = new QWidget(this);
    // rightPanel->setFixedWidth(400); // Removed fixed width
    rightPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(4);
    
    for(int i = 0; i < 2; i++) {
        QString name = (i == 0) ? "RC Car - Front Cam" : "RC Car - SLAM Lidar Map";
        
        // Wrap QLabel in a styled-card looking widget
        QWidget *card = new QWidget(rightPanel);
        card->setObjectName("SensorCard");
        // Style moved to QSS
        
        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(0, 0, 0, 0);
        
        // Header
        QWidget *header = new QWidget(card);
        header->setObjectName("SensorHeader");
        header->setFixedHeight(30);
        
        QHBoxLayout *headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(8, 0, 8, 0);
        QLabel *title = new QLabel(name, header);
        title->setObjectName("SensorTitle");
        title->setStyleSheet("font-weight: bold; font-size: 11px;");
        headerLayout->addWidget(title);
        
        // Content
        sensorWidgets[i] = new QLabel(name, card);
        sensorWidgets[i]->setObjectName("SensorValue");
        sensorWidgets[i]->setAlignment(Qt::AlignCenter);
        sensorWidgets[i]->setStyleSheet("background-color: transparent; font-size: 14px;"); // Colors in QSS
        sensorWidgets[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        
        cardLayout->addWidget(header);
        cardLayout->addWidget(sensorWidgets[i]);
        
        rightLayout->addWidget(card);
    }

    // Add to main layout
    // Grid takes 2/3, Right Panel takes 1/3 -> Equal column widths
    mainLayout->addWidget(gridContainer, 2); 
    mainLayout->addWidget(rightPanel, 1);

    if (!gst_is_initialized()) gst_init(nullptr, nullptr);
    streamStarted = false;
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

    lowQualityUrls.clear();
    highQualityUrls.clear();

    // =================================================================
    // [옵션 A] 라즈베리파이 / 내 서버
    // =================================================================
    QString ip = "192.168.0.39";
    QString port = "8554";

    for(int i = 0; i < 4; i++) {
        QString lowUrl = QString("rtsp://%1:%2/ch%3").arg(ip, port).arg(i+1);
        lowQualityUrls << lowUrl;
        QString highUrl = QString("rtsp://%1:%2/ch%3_fhd").arg(ip, port).arg(i+1);
        highQualityUrls << highUrl;
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
}

bool LiveView::eventFilter(QObject *obj, QEvent *event) {
    // Keep event filter if we want to handle other things, 
    // but fullscreen is now handled by VideoCard signal.
    return QWidget::eventFilter(obj, event);
}

void LiveView::setChannelVisible(int index, bool visible) {
    if (index < 4) {
        cctvWidgets[index]->setVisible(visible);
        updateCCTVLayout(); // Recalculate grid
    }
    else if (index < 6) {
        if(sensorWidgets[index - 4] && sensorWidgets[index - 4]->parentWidget())
            sensorWidgets[index - 4]->parentWidget()->setVisible(visible);
        
        // Check if both sensors are hidden
        bool anySensorVisible = false;
        for(int i=0; i<2; i++) {
            if(sensorWidgets[i] && sensorWidgets[i]->parentWidget() && !sensorWidgets[i]->parentWidget()->isHidden()) {
                anySensorVisible = true;
                break;
            }
        }
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
}
