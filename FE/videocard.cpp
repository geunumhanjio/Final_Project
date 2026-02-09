#include "videocard.h"
#include <QEvent>
#include <QMouseEvent>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

VideoCard::VideoCard(QWidget *parent) : QWidget(parent)
{
    setupUi();
    m_isHovered = false;
    showRecIndicator(true); // Default to showing REC for demo
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
    m_videoWidget = new VideoWidget(videoContainer);
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
    m_btnFullscreen->setFixedSize(24, 24);
    m_btnSettings->setFixedSize(24, 24);
    m_btnFullscreen->setStyleSheet("QPushButton { background-color: rgba(0,0,0,100); color: white; border: none; border-radius: 4px; } QPushButton:hover { background-color: #135bec; }");
    m_btnSettings->setStyleSheet("QPushButton { background-color: rgba(0,0,0,100); color: white; border: none; border-radius: 4px; } QPushButton:hover { background-color: #135bec; }");

    topLayout->addWidget(m_recBadge);
    topLayout->addStretch();
    topLayout->addWidget(m_btnFullscreen);
    topLayout->addWidget(m_btnSettings);
    
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
