#include "videocard.h"
#include "goaloverlaycontroller.h"

#include "livevideowidget.h"
#include "osdwidget.h"   // [New]

#include <cmath>
#include <QEvent>
#include <QMouseEvent>
#include <QMenu>         // [New]
#include <QAction>       // [New]
#include <QCheckBox>     // [New]
#include <QLabel>        // [New]
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QPropertyAnimation>

VideoCard::VideoCard(QWidget *parent) : QWidget(parent)
{
    setupUi();
    m_isHovered = false;
    m_isRecording = false;
    m_channelId = -1;
    m_goalOverlaySyncTimer = new QTimer(this);
    m_goalOverlaySyncTimer->setInterval(16);
    connect(m_goalOverlaySyncTimer, &QTimer::timeout, this, &VideoCard::syncGoalOverlayPosition);
    m_goalOverlaySyncTimer->start();
    showRecIndicator(false); // Default hidden until recording start
}

VideoCard::~VideoCard()
{
    delete m_goalOverlay;
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
    m_videoWidget->installEventFilter(this);
    videoLayout->addWidget(m_videoWidget, 0, 0);

    GoalOverlay::ArrowStyle overlayStyle;
    overlayStyle.penWidth = 3.0;
    overlayStyle.arrowSize = 12.0;
    m_goalOverlay = new GoalArrowOverlayWidget(overlayStyle, window());

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

void VideoCard::setGoalTargetingEnabled(bool enabled)
{
    m_goalTargetingEnabled = enabled;
    if (!m_goalOverlay) {
        return;
    }

    m_isSettingGoalDirection = false;
    m_goalStartPos = QPointF();
    m_goalEndPos = QPointF();
    m_videoWidget->setMouseTracking(enabled);
    m_goalOverlay->syncToWidget(m_videoWidget);
    m_goalOverlay->setClipRect(m_videoWidget->getVideoDisplayRect());
    m_goalOverlay->clearPreview();
    m_goalOverlay->setActive(enabled);
    updateCommittedGoalOverlay();
    m_topOverlay->setVisible(m_isHovered && !enabled);
}

bool VideoCard::isGoalTargetingEnabled() const
{
    return m_goalTargetingEnabled;
}

void VideoCard::clearGoalOverlay()
{
    m_isSettingGoalDirection = false;
    m_goalStartPos = QPointF();
    m_goalEndPos = QPointF();
    m_hasCommittedGoalOverlay = false;
    m_committedGoalStartNormalized = QPointF();
    m_committedGoalEndNormalized = QPointF();
    m_hasCommittedGoalLocalCache = false;
    m_committedGoalStartLocal = QPointF();
    m_committedGoalEndLocal = QPointF();
    m_committedGoalDisplayRect = QRectF();
    m_committedGoalCropRect = QRectF();
    m_preserveLocalCommittedGoalOnNextSet = false;
    if (m_goalOverlay) {
        m_goalOverlay->clearAll();
    }
}

void VideoCard::setCommittedGoalOverlay(const QPointF &normalizedStart, const QPointF &normalizedEnd)
{
    const bool preserveLocalCache = m_preserveLocalCommittedGoalOnNextSet
                                    && m_hasCommittedGoalOverlay
                                    && GoalOverlay::samePoint(m_committedGoalStartNormalized, normalizedStart, GoalOverlay::kPointCacheEpsilon)
                                    && GoalOverlay::samePoint(m_committedGoalEndNormalized, normalizedEnd, GoalOverlay::kPointCacheEpsilon);
    m_preserveLocalCommittedGoalOnNextSet = false;
    m_hasCommittedGoalOverlay = true;
    m_committedGoalStartNormalized = normalizedStart;
    m_committedGoalEndNormalized = normalizedEnd;
    if (!preserveLocalCache) {
        m_hasCommittedGoalLocalCache = false;
        m_committedGoalStartLocal = QPointF();
        m_committedGoalEndLocal = QPointF();
        m_committedGoalDisplayRect = QRectF();
        m_committedGoalCropRect = QRectF();
    }
    updateCommittedGoalOverlay();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void VideoCard::enterEvent(QEnterEvent *event)
#else
void VideoCard::enterEvent(QEvent *event)
#endif
{
    m_isHovered = true;
    m_topOverlay->setVisible(!m_goalTargetingEnabled);
    QWidget::enterEvent(event);
}

bool VideoCard::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_videoWidget && m_goalTargetingEnabled && m_goalOverlay) {
        GoalArrowOverlayWidget *overlay = m_goalOverlay;

        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton && m_videoWidget->width() > 0 && m_videoWidget->height() > 0) {
                overlay->syncToWidget(m_videoWidget);
                const QRectF displayRect = m_videoWidget->getVideoDisplayRect();
                overlay->setClipRect(displayRect);

                bool ok = false;
                m_videoWidget->widgetPointToVideoNormalized(mouseEvent->position(), &ok);
                if (!ok) {
                    return true;
                }

                emit goalInteractionStarted();
                m_goalStartPos = mouseEvent->position();
                m_goalEndPos = m_goalStartPos;
                m_isSettingGoalDirection = true;
                m_hasCommittedGoalOverlay = false;
                overlay->setPreviewArrow(m_goalStartPos, m_goalEndPos);
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_isSettingGoalDirection) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            const QRectF displayRect = m_videoWidget->getVideoDisplayRect();
            m_goalEndPos = GoalOverlay::clampPointToRect(mouseEvent->position(), displayRect);
            overlay->setPreviewArrow(m_goalStartPos, m_goalEndPos);
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton && m_isSettingGoalDirection) {
                const std::optional<GoalOverlay::CommitResult> commit =
                    GoalOverlay::buildCommitResult(m_videoWidget, m_goalStartPos, mouseEvent->position());
                if (!commit.has_value()) {
                    m_isSettingGoalDirection = false;
                    m_goalStartPos = QPointF();
                    m_goalEndPos = QPointF();
                    overlay->clearPreview();
                    return true;
                }

                m_goalEndPos = commit->localEnd;
                m_hasCommittedGoalOverlay = true;
                m_committedGoalStartNormalized = commit->normalizedStart;
                m_committedGoalEndNormalized = commit->normalizedEnd;
                m_hasCommittedGoalLocalCache = true;
                m_committedGoalStartLocal = commit->localStart;
                m_committedGoalEndLocal = commit->localEnd;
                m_committedGoalDisplayRect = commit->displayRect;
                m_committedGoalCropRect = commit->cropRect;
                m_preserveLocalCommittedGoalOnNextSet = true;
                overlay->commitArrow(commit->localStart, commit->localEnd);
                emit goalOverlayCommitted(commit->normalizedStart, commit->normalizedEnd);
                emit goalCommitted();
                emit goalRequested(commit->normalizedStart, commit->angleRadians);

                m_isSettingGoalDirection = false;
                m_goalStartPos = QPointF();
                m_goalEndPos = QPointF();
                return true;
            }
        } else if (event->type() == QEvent::Leave && m_isSettingGoalDirection) {
            overlay->setPreviewArrow(m_goalStartPos, m_goalStartPos);
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
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
    syncGoalOverlayPosition();
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

void VideoCard::syncGoalOverlayPosition()
{
    if (!m_goalOverlay || !m_videoWidget) {
        return;
    }

    updateCommittedGoalOverlay();

    if (!isVisible()) {
        if (m_goalOverlay->isVisible()) {
            m_goalOverlay->hide();
        }
        return;
    }

    if (!m_goalOverlay->isVisible()) {
        return;
    }

    m_goalOverlay->syncToWidget(m_videoWidget);
    m_goalOverlay->setClipRect(m_videoWidget->getVideoDisplayRect());
}

void VideoCard::updateCommittedGoalOverlay()
{
    if (!m_goalOverlay || !m_videoWidget || m_isSettingGoalDirection) {
        return;
    }

    GoalArrowOverlayWidget *overlay = m_goalOverlay;
    if (!isVisible()) {
        overlay->hide();
        return;
    }

    overlay->syncToWidget(m_videoWidget);
    const QRectF displayRect = m_videoWidget->getVideoDisplayRect();
    const QRectF cropRect = m_videoWidget->getCurrentCrop();
    overlay->setClipRect(displayRect);
    if (!m_hasCommittedGoalOverlay || m_videoWidget->width() <= 0 || m_videoWidget->height() <= 0) {
        overlay->setCommittedArrow(QPointF(), QPointF(), false);
        return;
    }

    if (m_hasCommittedGoalLocalCache
        && GoalOverlay::sameRect(displayRect, m_committedGoalDisplayRect, GoalOverlay::kDisplayRectCacheEpsilon)
        && GoalOverlay::sameRect(cropRect, m_committedGoalCropRect, GoalOverlay::kCropRectCacheEpsilon)) {
        overlay->setCommittedArrow(m_committedGoalStartLocal, m_committedGoalEndLocal, true);
        return;
    }

    const std::optional<GoalOverlay::ProjectionResult> projection =
        GoalOverlay::projectCommittedGoal(m_videoWidget,
                                          m_committedGoalStartNormalized,
                                          m_committedGoalEndNormalized);
    if (!projection.has_value()) {
        overlay->setCommittedArrow(QPointF(), QPointF(), false);
        return;
    }

    m_hasCommittedGoalLocalCache = true;
    m_committedGoalStartLocal = projection->localStart;
    m_committedGoalEndLocal = projection->localEnd;
    m_committedGoalDisplayRect = projection->displayRect;
    m_committedGoalCropRect = projection->cropRect;
    overlay->setCommittedArrow(projection->localStart, projection->localEnd, true);
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

