#include "videocard.h"
#include <QEvent>
#include <QMouseEvent>
#include <QMenu>         // [New]
#include <QAction>       // [New]
#include <QCheckBox>     // [New]
#include <QLabel>        // [New]
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include "livevideowidget.h"
#include "osdwidget.h"   // [New]

VideoCard::VideoCard(QWidget *parent) : QWidget(parent)
{
    setupUi();
    m_isHovered = false;
    m_isRecording = false;
    m_channelId = -1;
    showRecIndicator(false); // Default hidden until recording start
}

VideoCard::~VideoCard()
{
}

void VideoCard::setupUi()
{
    // 1. Main Layout (Vertical)
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 2. Video Container (Relative positioning for overlays)
    QWidget *videoContainer = new QWidget(this);
    videoContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    videoContainer->setStyleSheet("background-color: black; border: 1px solid #2a3649; border-radius: 4px;");
    
    // Grid Layout for Video Container to stack overlays
    QGridLayout *videoLayout = new QGridLayout(videoContainer);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    
    // 2-1. The Video Widget
    m_videoWidget = new LiveVideoWidget(videoContainer);
    m_videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    videoLayout->addWidget(m_videoWidget, 0, 0);

    // 2-2. Top Overlay (REC badge, Buttons)
    m_topOverlay = new QWidget(videoContainer);
    m_topOverlay->setFixedHeight(40);
    m_topOverlay->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(0,0,0,150), stop:1 rgba(0,0,0,0));");
    
    QHBoxLayout *topLayout = new QHBoxLayout(m_topOverlay);
    topLayout->setContentsMargins(8, 8, 8, 0);
    
    // REC Badge
    m_recBadge = new QWidget(m_topOverlay);
    m_recBadge->setFixedSize(50, 20);
    m_recBadge->setStyleSheet("background-color: #dc2626; border-radius: 3px;");
    QHBoxLayout *recLayout = new QHBoxLayout(m_recBadge);
    recLayout->setContentsMargins(4, 0, 4, 0);
    recLayout->setSpacing(4);
    
    QLabel *recDot = new QLabel(m_recBadge);
    recDot->setFixedSize(6, 6);
    recDot->setStyleSheet("background-color: white; border-radius: 3px;");
    
    m_recLabel = new QLabel("REC", m_recBadge);
    m_recLabel->setStyleSheet("color: white; font-weight: bold; font-size: 10px; border: none; background: transparent;");
    
    recLayout->addWidget(recDot);
    recLayout->addWidget(m_recLabel);
    
    // Action Buttons
    m_btnFullscreen = new QPushButton("⛶", m_topOverlay);
    m_btnSettings = new QPushButton("⚙", m_topOverlay);
    m_btnRecord = new QPushButton("●", m_topOverlay); // [New] Rec Button
    
    m_btnFullscreen->setFixedSize(24, 24);
    m_btnSettings->setFixedSize(24, 24);
    m_btnRecord->setFixedSize(24, 24);

    m_btnFullscreen->setStyleSheet("QPushButton { background-color: rgba(0,0,0,100); color: white; border: none; border-radius: 4px; } QPushButton:hover { background-color: #135bec; }");
    m_btnSettings->setStyleSheet("QPushButton { background-color: rgba(0,0,0,100); color: white; border: none; border-radius: 4px; } QPushButton:hover { background-color: #135bec; }");
    m_btnRecord->setStyleSheet("QPushButton { background-color: rgba(255,0,0,180); color: white; border: none; border-radius: 12px; } QPushButton:hover { background-color: #ff4444; }");
    
    // [Fix] Prevent buttons from stealing focus
    m_btnFullscreen->setFocusPolicy(Qt::NoFocus);
    m_btnSettings->setFocusPolicy(Qt::NoFocus);
    m_btnRecord->setFocusPolicy(Qt::NoFocus);

    topLayout->addWidget(m_recBadge);
    topLayout->addWidget(m_btnRecord); // Add Record Button next to badge
    topLayout->addStretch();
    topLayout->addWidget(m_btnFullscreen);
    topLayout->addWidget(m_btnSettings);
    
    connect(m_btnRecord, &QPushButton::clicked, this, &VideoCard::toggleRecord);
    
    // 2-3. Bottom In-Video Overlay (Resolution Info) - Removed as requested
    /*
    m_bottomOverlay = new QWidget(videoContainer);
    m_bottomOverlay->setFixedHeight(30);
    
    QHBoxLayout *bottomOverlayLayout = new QHBoxLayout(m_bottomOverlay);
    bottomOverlayLayout->setContentsMargins(0, 0, 8, 4);
    bottomOverlayLayout->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    
    m_streamInfoLabel = new QLabel("CAM 01 | 1080p | 30fps", m_bottomOverlay);
    m_streamInfoLabel->setStyleSheet("background-color: rgba(0,0,0,150); color: #e2e8f0; font-family: monospace; font-size: 10px; padding: 2px 6px; border-radius: 4px;");
    bottomOverlayLayout->addWidget(m_streamInfoLabel);

    videoLayout->addWidget(m_bottomOverlay, 0, 0, Qt::AlignBottom);
    */

    // Add overlays to grid (on top of video)
    videoLayout->addWidget(m_topOverlay, 0, 0, Qt::AlignTop);
    // videoLayout->addWidget(m_bottomOverlay, 0, 0, Qt::AlignBottom); // Removed

    // 3. Status Bar (Below Video)
    m_statusBar = new QFrame(this);
    m_statusBar->setObjectName("VideoCardStatusBar");
    m_statusBar->setFixedHeight(32);
    // Style moved to QSS
    
    QHBoxLayout *statusLayout = new QHBoxLayout(m_statusBar);
    statusLayout->setContentsMargins(12, 0, 12, 0);
    
    m_statusIcon = new QLabel("●", m_statusBar);
    m_statusIcon->setStyleSheet("color: #22c55e; font-size: 12px; margin-right: 4px; border:none; background:transparent;");
    
    m_channelLabel = new QLabel("Channel Name", m_statusBar);
    m_channelLabel->setObjectName("VideoCardChannelLabel");
    m_channelLabel->setStyleSheet("font-size: 11px; font-weight: 600; border:none; background:transparent;");
    
    // m_muteIcon removed as requested

    statusLayout->addWidget(m_statusIcon);
    statusLayout->addWidget(m_channelLabel);
    statusLayout->addStretch();
    // statusLayout->addWidget(m_muteIcon);

    // Assemble
    mainLayout->addWidget(videoContainer);
    mainLayout->addWidget(m_statusBar);

    // Initial State
    m_topOverlay->setVisible(false);
    
    // Connect Fullscreen Signal
    connect(m_btnFullscreen, &QPushButton::clicked, this, &VideoCard::fullScreenRequested);
    
    // [New] Connect Settings (OSD Menu) Signal
    connect(m_btnSettings, &QPushButton::clicked, this, &VideoCard::showSettingsMenu);
}

void VideoCard::playUrl(const QString &url, int latency)
{
    m_videoWidget->playUrl(url, latency);
}

void VideoCard::stop()
{
    m_videoWidget->stop();
}

void VideoCard::setChannelName(const QString &name)
{
    m_channelLabel->setText(name);
}

void VideoCard::setChannelStatus(bool active)
{
    m_statusIcon->setStyleSheet(active ? "color: #22c55e; border:none; background:transparent;" : "color: #ef4444; border:none; background:transparent;");
}

void VideoCard::setStreamInfo(const QString &info)
{
    m_streamInfoLabel->setText(info);
}

void VideoCard::showRecIndicator(bool show)
{
    m_recBadge->setVisible(show);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void VideoCard::enterEvent(QEnterEvent *event)
#else
void VideoCard::enterEvent(QEvent *event)
#endif
{
    m_isHovered = true;
    m_topOverlay->setVisible(true);
    QWidget::enterEvent(event);
}

void VideoCard::leaveEvent(QEvent *event)
{
    m_isHovered = false;
    m_topOverlay->setVisible(false);
    QWidget::leaveEvent(event);
}

void VideoCard::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void VideoCard::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    emit fullScreenRequested();
}

void VideoCard::updateOverlayLayout()
{
    // If we wanted to do manual positioning instead of Grid, we would do it here.
}

void VideoCard::toggleRecord()
{
    m_isRecording = !m_isRecording;
    
    if (m_isRecording) {
        // Start Recording
        showRecIndicator(true);
        m_btnRecord->setText("■"); // Stop Square
        m_btnRecord->setStyleSheet("QPushButton { background-color: rgba(0,0,0,150); color: white; border: 1px solid white; border-radius: 4px; } QPushButton:hover { background-color: #444; }");
        if (m_channelId != -1) emit recordRequested(m_channelId, true);
    } else {
        // Stop Recording
        showRecIndicator(false);
        m_btnRecord->setText("●"); // Rec Circle
        m_btnRecord->setStyleSheet("QPushButton { background-color: rgba(255,0,0,180); color: white; border: none; border-radius: 12px; } QPushButton:hover { background-color: #ff4444; }");
        if (m_channelId != -1) emit recordRequested(m_channelId, false);
    }
}

// [New] Settings 메뉴 설정 함수
void VideoCard::showSettingsMenu()
{
    if (!m_videoWidget || !m_videoWidget->getOsdWidget()) return;
    
    QWidget *popup = new QWidget(this);
    popup->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setStyleSheet("QWidget { background-color: #2b2b2b; color: white; border: 1px solid #555; border-radius: 4px; } "
                         "QCheckBox { spacing: 8px; font-size: 11px; padding: 4px; border: none; } "
                         "QCheckBox::indicator { width: 14px; height: 14px; } "
                         "QLabel { border: none; font-weight: bold; font-size: 12px; }");

    QVBoxLayout *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    // Header with Title and X button
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *title = new QLabel("OSD Settings", popup);
    QPushButton *closeBtn = new QPushButton("✕", popup);
    closeBtn->setFixedSize(20, 20);
    closeBtn->setStyleSheet("QPushButton { border: none; background: transparent; color: #aaa; font-weight: bold; } "
                            "QPushButton:hover { color: white; background: rgba(255,0,0,150); border-radius: 2px; }");
    connect(closeBtn, &QPushButton::clicked, popup, &QWidget::close);

    headerLayout->addWidget(title);
    headerLayout->addStretch();
    headerLayout->addWidget(closeBtn);
    layout->addLayout(headerLayout);

    // Divider
    QFrame *line = new QFrame(popup);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("border: 1px solid #555;");
    layout->addWidget(line);

    OsdWidget *osd = m_videoWidget->getOsdWidget();

    // [New] All Checkbox
    QCheckBox *cbAll = new QCheckBox("All", popup);
    cbAll->setStyleSheet("font-weight: bold; color: #F59E0B;"); // highlight
    layout->addWidget(cbAll);

    // Divider
    QFrame *line2 = new QFrame(popup);
    line2->setFrameShape(QFrame::HLine);
    line2->setStyleSheet("border: 1px solid #555;");
    layout->addWidget(line2);

    QList<QPair<OsdWidget::Metric, QCheckBox*>> metricCbs;
    int checkedCount = 0;

    for (int i = 0; i < OsdWidget::MetricCount; ++i) {
        OsdWidget::Metric metric = static_cast<OsdWidget::Metric>(i);
        QCheckBox *cb = new QCheckBox(OsdWidget::getMetricName(metric), popup);
        bool isVis = osd->isMetricVisible(metric);
        cb->setChecked(isVis);
        if (isVis) checkedCount++;
        
        metricCbs.append(qMakePair(metric, cb));
        layout->addWidget(cb);
    }

    // Now connect after all cb are populated
    for (auto pair : metricCbs) {
        OsdWidget::Metric metric = pair.first;
        QCheckBox *cb = pair.second;
        
        connect(cb, &QCheckBox::toggled, [this, osd, metric, cbAll, metricCbs](bool checked) {
            osd->setMetricVisible(metric, checked);
            if (checked) osd->show();
            
            // Check if all are checked to update cbAll
            bool allChecked = true;
            bool anyChecked = false; // [New]
            for(auto p : metricCbs) {
                if(!p.second->isChecked()) { allChecked = false; }
                if(p.second->isChecked()) { anyChecked = true; }
            }
            cbAll->blockSignals(true);
            cbAll->setChecked(allChecked);
            cbAll->blockSignals(false);
            
            Q_UNUSED(anyChecked);
            // Emit signal to dynamically connect to camera server WebSocket
            emit streamStatsRequested(m_channelId, anyChecked);
        });
    }

    // Initialize All checkbox state
    cbAll->setChecked(checkedCount == OsdWidget::MetricCount);
    
    connect(cbAll, &QCheckBox::toggled, [metricCbs](bool checked) {
        for(auto p : metricCbs) {
            if (p.second->isChecked() != checked) {
                p.second->setChecked(checked); // this will trigger individual toggled signals
            }
        }
    });

    // 톱니바퀴 버튼 아래에 메뉴 띄우기
    QPoint globalPos = m_btnSettings->mapToGlobal(QPoint(0, m_btnSettings->height()));
    popup->move(globalPos);
    popup->show();
}

void VideoCard::updateStreamStats(double fps, double bitrateKbps, double proxyLatencyMs)
{
    if (m_videoWidget && m_videoWidget->getOsdWidget()) {
        OsdWidget *osd = m_videoWidget->getOsdWidget();
        
        if (osd->isMetricVisible(OsdWidget::FPS)) {
            osd->setMetricValue(OsdWidget::FPS, QString::number(qRound(fps)));
        }
        if (osd->isMetricVisible(OsdWidget::Bitrate)) {
            osd->setMetricValue(OsdWidget::Bitrate, QString::number(bitrateKbps / 1024.0, 'f', 2) + " Mbps");
        }
        if (osd->isMetricVisible(OsdWidget::Latency)) {
            osd->setMetricValue(OsdWidget::Latency, QString::number(proxyLatencyMs, 'f', 3) + " ms");
        }
    }
}
