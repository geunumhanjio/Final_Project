#include "fullscreenview.h"
#include "goaloverlaycontroller.h"

#include <cmath>
#include <QDebug>
#include <QCursor>
#include <QPainter>
#include <QFileInfo>
#include <QMenu>         // [New]
#include <QAction>       // [New]
#include <QGridLayout>
#include <QButtonGroup>
#include <QCheckBox>     // [New]
#include <QLabel>        // [New]
#include <QRadioButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QWheelEvent>
#include "osdwidget.h"   // [New]
#include <QGuiApplication>
#include <QApplication>

namespace {

constexpr double kWheelZoomStepFactor = 1.2;
constexpr double kMinCropExtent = 0.001;

QRectF selectionRectToCropRect(VideoWidget *videoWidget, const QRect &selectionRect)
{
    if (!videoWidget || selectionRect.width() < 2 || selectionRect.height() < 2) {
        return QRectF();
    }

    const QRectF displayRect = videoWidget->getVideoDisplayRect();
    const QRectF currentCrop = videoWidget->getCurrentCrop();
    if (displayRect.isEmpty() || currentCrop.width() <= 0.0 || currentCrop.height() <= 0.0) {
        return QRectF();
    }

    const QRectF widgetSelectionRect(QPointF(selectionRect.x(), selectionRect.y()),
                                     QPointF(selectionRect.x() + selectionRect.width(),
                                             selectionRect.y() + selectionRect.height()));
    const QRectF clippedSelectionRect = widgetSelectionRect.normalized().intersected(displayRect);
    if (clippedSelectionRect.width() <= 1.0 || clippedSelectionRect.height() <= 1.0) {
        return QRectF();
    }

    const double leftRatio = qBound(0.0,
                                    (clippedSelectionRect.left() - displayRect.left()) / displayRect.width(),
                                    1.0);
    const double topRatio = qBound(0.0,
                                   (clippedSelectionRect.top() - displayRect.top()) / displayRect.height(),
                                   1.0);
    const double rightRatio = qBound(0.0,
                                     (clippedSelectionRect.right() - displayRect.left()) / displayRect.width(),
                                     1.0);
    const double bottomRatio = qBound(0.0,
                                      (clippedSelectionRect.bottom() - displayRect.top()) / displayRect.height(),
                                      1.0);

    const double targetX = currentCrop.x() + (leftRatio * currentCrop.width());
    const double targetY = currentCrop.y() + (topRatio * currentCrop.height());
    const double targetW = qMax(0.0, rightRatio - leftRatio) * currentCrop.width();
    const double targetH = qMax(0.0, bottomRatio - topRatio) * currentCrop.height();
    return QRectF(targetX, targetY, targetW, targetH);
}

} // namespace

// =============================================================
// [내부 클래스] 직접 그려지는 네모 박스
// =============================================================
class SimpleRubberBand : public QWidget {
public:
    SimpleRubberBand(QWidget* parent = nullptr) : QWidget(parent) {
        // 1. 테두리 없는 탑레벨 투명 윈도우 설정
        setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowDoesNotAcceptFocus);

        // 2. 배경 투명화 (중요)
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);

        // [수정] 시스템 배경 그리기 방지 설정을 true로 변경!
        // (이게 false면 윈도우가 기본 배경색(흰색)을 칠해버립니다.)
        setAttribute(Qt::WA_NoSystemBackground, true);
    }

protected:
    // 직접 그리기 (빨간 테두리만)
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // 빨간색 펜, 두께 3
        QPen pen(Qt::red);
        pen.setWidth(3);
        p.setPen(pen);

        // [중요] 채우기 없음 (투명)
        p.setBrush(Qt::NoBrush);

        // 화면 꽉 차게 네모 그리기
        p.drawRect(rect().adjusted(1, 1, -2, -2));
    }
};

// =============================================================
// [내부 클래스] 조종 모드용 직접 그려지는 오버레이 (원, 화살표)
// =============================================================
#if 0
class ControlOverlay : public QWidget {
public:
    QPointF startPos;
    QPointF endPos;
    bool isDrawingArrow;
    QPointF committedStartPos;
    QPointF committedEndPos;
    bool hasCommittedArrow;

    ControlOverlay(QWidget* parent = nullptr) : QWidget(parent) {
        // [수정] SimpleRubberBand처럼 비디오 위젯 위에 확실히 그려지도록 탑레벨 윈도우 속성 부여
        setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowDoesNotAcceptFocus);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_ShowWithoutActivating, true);
        
        isDrawingArrow = false;
        hasCommittedArrow = false;
    }

    void setCommittedArrow(const QPointF &start, const QPointF &end, bool hasArrow) {
        const bool changed = (hasCommittedArrow != hasArrow)
                             || (committedStartPos != start)
                             || (committedEndPos != end);
        hasCommittedArrow = hasArrow;
        committedStartPos = hasArrow ? start : QPointF();
        committedEndPos = hasArrow ? end : QPointF();
        updateVisibility();
        if (changed) {
            update();
        }
    }

    void setClipRect(const QRectF &clipRect) {
        m_clipRect = clipRect;
        update();
    }

    void commitArrow(const QPointF &start, const QPointF &end) {
        startPos = QPointF();
        endPos = QPointF();
        isDrawingArrow = false;
        setCommittedArrow(start, end, true);
    }

    void clearPreview() {
        startPos = QPointF();
        endPos = QPointF();
        isDrawingArrow = false;
        updateVisibility();
        update();
    }

    void clearAll() {
        clearPreview();
        setCommittedArrow(QPointF(), QPointF(), false);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setClipRect(m_clipRect.isValid() ? m_clipRect : QRectF(rect()));

        const auto drawArrow = [&p](const QPointF &start, const QPointF &end) {
            if (start.isNull()) {
                return;
            }

            QPen pen(Qt::red);
            pen.setWidth(4);
            p.setPen(pen);
            p.setBrush(Qt::red);
            p.drawEllipse(start, 5.0, 5.0);

            if (end.isNull() || end == start) {
                return;
            }

            p.drawLine(start, end);

            const double angle = std::atan2(end.y() - start.y(), end.x() - start.x());
            const double arrowSize = 15.0;
            const QPointF p1 = end - QPointF(arrowSize * std::cos(angle - (M_PI / 6.0)),
                                             arrowSize * std::sin(angle - (M_PI / 6.0)));
            const QPointF p2 = end - QPointF(arrowSize * std::cos(angle + (M_PI / 6.0)),
                                             arrowSize * std::sin(angle + (M_PI / 6.0)));
            QPolygonF arrowHead;
            arrowHead << end << p1 << p2;
            p.drawPolygon(arrowHead);
        };

        if (hasCommittedArrow) {
            drawArrow(committedStartPos, committedEndPos);
        }
        if (isDrawingArrow) {
            drawArrow(startPos, endPos);
        }
        return;

        QPen pen(Qt::red);
        pen.setWidth(4);
        p.setPen(pen);
        p.setBrush(Qt::red);

        // 첫 번째 클릭 지점(목표 위치)
        p.drawEllipse(startPos, 5, 5);

        if (isDrawingArrow && !endPos.isNull() && startPos != endPos) {
            p.drawLine(startPos, endPos);

            // 화살 촉 계산
            double angle = std::atan2(endPos.y() - startPos.y(), endPos.x() - startPos.x());
            double arrowSize = 15;
            QPointF p1 = endPos - QPointF(arrowSize * std::cos(angle - M_PI / 6),
                                          arrowSize * std::sin(angle - M_PI / 6));
            QPointF p2 = endPos - QPointF(arrowSize * std::cos(angle + M_PI / 6),
                                          arrowSize * std::sin(angle + M_PI / 6));
            QPolygonF arrowHead;
            arrowHead << endPos << p1 << p2;
            p.drawPolygon(arrowHead);
        }
    }

private:
    void updateVisibility() {
        if (hasCommittedArrow || isDrawingArrow) {
            if (!isVisible()) {
                show();
            }
        } else if (isVisible()) {
            hide();
        }
    }

    QRectF m_clipRect;
};
// =============================================================


#endif
FullScreenView::FullScreenView(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    this->setStyleSheet("background-color: black;");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 영상 컨테이너 (그대로 유지)
    QWidget *videoContainer = new QWidget(this);
    videoContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // [수정] Grid(Overlay) -> VBox(Stack) 변경
    QVBoxLayout *videoLayout = new QVBoxLayout(videoContainer);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    videoLayout->setSpacing(0);

    // 1. Top Info Bar (Stacked Top)
    topBar = new QWidget(videoContainer);
    topBar->setObjectName("FS_TopBar"); // [New]
    topBar->setFixedHeight(80);
    // Initial Theme (matches updateTheme(true))
    
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(20, 10, 20, 10); // Bottom margin reduced (30->10)
    
    titleLabel = new QLabel("CCTV Camera", topBar);
    titleLabel->setObjectName("FS_TitleLabel"); // [New]
    titleLabel->setStyleSheet("color: #F59E0B; font-weight: bold; font-size: 24px;"); // Orange like the control button
    
    liveBadge = new QLabel("LIVE", topBar);
    liveBadge->setObjectName("FS_LiveBadge"); // [New]
    
    // [New] Settings Button for OSD Menu
    btnSettings = new QPushButton("⚙", topBar);
    btnSettings->setObjectName("FS_SettingsBtn");
    btnSettings->setFixedSize(36, 36);
    btnSettings->setCursor(Qt::PointingHandCursor);
    
    btnClose = new QPushButton("✕", topBar);
    btnClose->setObjectName("FS_CloseBtn"); // [New]
    btnClose->setFixedSize(36, 36);
    btnClose->setCursor(Qt::PointingHandCursor);

    // [New] Connect Settings (OSD Menu) Signal
    connect(btnSettings, &QPushButton::clicked, this, [this](){
        if (!videoWidget || !videoWidget->getOsdWidget()) return;
        
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

        OsdWidget *osd = videoWidget->getOsdWidget();

        // [New] All Checkbox
        QCheckBox *cbAll = new QCheckBox("All", popup);
        cbAll->setStyleSheet("font-weight: bold; color: #F59E0B;");
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

        for (auto pair : metricCbs) {
            OsdWidget::Metric metric = pair.first;
            QCheckBox *cb = pair.second;
            
            connect(cb, &QCheckBox::toggled, [this, osd, metric, cbAll, metricCbs](bool checked) {
                osd->setMetricVisible(metric, checked);
                if (checked) osd->show();

                // Update 'All' checkbox state
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

        cbAll->setChecked(checkedCount == OsdWidget::MetricCount);
        
        connect(cbAll, &QCheckBox::toggled, [metricCbs](bool checked) {
            for(auto p : metricCbs) {
                if (p.second->isChecked() != checked) {
                    p.second->setChecked(checked);
                }
            }
        });

        QPoint globalPos = btnSettings->mapToGlobal(QPoint(0, btnSettings->height()));
        popup->move(globalPos);
        popup->show();
    });

    connect(btnClose, &QPushButton::clicked, this, [this]() {
        requestCloseView();
    });

    topLayout->addWidget(titleLabel);
    topLayout->addSpacing(10);
    topLayout->addWidget(liveBadge);
    topLayout->addStretch();
    topLayout->addWidget(btnSettings); // [New] Add to layout
    topLayout->addWidget(btnClose);

    // Add TopBar to VBox
    videoLayout->addWidget(topBar);

    // 2. Video Layer (Stacked Middle) with Stretch
    // 2. Video Layer (Stacked Middle) with Stretch
    videoStack = new QStackedWidget(videoContainer);
    videoStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    liveWidget = new LiveVideoWidget(videoStack);
    liveWidget->installEventFilter(this);
    
    recordedWidget = new RecordedVideoWidget(videoStack);
    recordedWidget->installEventFilter(this);
    
    videoStack->addWidget(liveWidget);
    videoStack->addWidget(recordedWidget);
    
    videoWidget = liveWidget; // Default
    
    videoLayout->addWidget(videoStack, 1); // stretch factor 1 to take available space

    // 3. Bottom Control Bar (Stacked Bottom)
    underBar = new FullUnderBar(videoContainer);
    modeQuickPanel = new QWidget(videoContainer);
    modeQuickPanel->setObjectName("FS_ModeQuickPanel");
    modeQuickPanel->setAttribute(Qt::WA_StyledBackground, true);
    modeQuickPanel->setStyleSheet(
        "#FS_ModeQuickPanel {"
        "  background-color: rgba(15, 23, 42, 0.82);"
        "  border: 1px solid rgba(255, 255, 255, 0.12);"
        "  border-radius: 12px;"
        "}"
        "#FS_ModeQuickTitle {"
        "  color: rgba(226, 232, 240, 0.78);"
        "  font-size: 10px;"
        "  font-weight: 700;"
        "  letter-spacing: 1px;"
        "}"
        "QRadioButton#FS_ModeQuickOption {"
        "  color: #E2E8F0;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  spacing: 6px;"
        "}"
        "QRadioButton#FS_ModeQuickOption::indicator {"
        "  width: 11px;"
        "  height: 11px;"
        "  border-radius: 6px;"
        "  border: 1px solid rgba(148, 163, 184, 0.75);"
        "  background: transparent;"
        "}"
        "QRadioButton#FS_ModeQuickOption::indicator:checked {"
        "  background: #F59E0B;"
        "  border: 1px solid #F59E0B;"
        "}"
        "QPushButton#FS_ModeQuickStopButton {"
        "  margin-top: 4px;"
        "  min-height: 28px;"
        "  padding: 0 10px;"
        "  border-radius: 8px;"
        "  border: 1px solid rgba(248, 113, 113, 0.42);"
        "  background-color: rgba(127, 29, 29, 0.9);"
        "  color: #FEE2E2;"
        "  font-size: 11px;"
        "  font-weight: 700;"
        "}"
        "QPushButton#FS_ModeQuickStopButton:disabled {"
        "  background-color: rgba(51, 65, 85, 0.72);"
        "  border: 1px solid rgba(148, 163, 184, 0.22);"
        "  color: rgba(226, 232, 240, 0.5);"
        "}"
        "QPushButton#FS_ModeQuickStopButton:hover:!disabled {"
        "  background-color: rgba(153, 27, 27, 0.95);"
        "}");

    QVBoxLayout *modeQuickLayout = new QVBoxLayout(modeQuickPanel);
    modeQuickLayout->setContentsMargins(12, 10, 12, 10);
    modeQuickLayout->setSpacing(4);

    modeQuickTitle = new QLabel("ROBOT", modeQuickPanel);
    modeQuickTitle->setObjectName("FS_ModeQuickTitle");
    modeQuickLayout->addWidget(modeQuickTitle);

    modeQuickGroup = new QButtonGroup(modeQuickPanel);
    modeQuickGroup->setExclusive(true);

    modeQuickManualButton = new QRadioButton("Manual", modeQuickPanel);
    modeQuickAutoButton = new QRadioButton("Auto", modeQuickPanel);
    modeQuickControlButton = new QRadioButton("Control", modeQuickPanel);
    modeQuickPatrolButton = new QRadioButton("Patrol", modeQuickPanel);
    for (QRadioButton *button : {modeQuickManualButton, modeQuickAutoButton, modeQuickControlButton, modeQuickPatrolButton}) {
        button->setObjectName("FS_ModeQuickOption");
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        modeQuickLayout->addWidget(button);
    }

    modeQuickStopButton = new QPushButton(QStringLiteral("정지"), modeQuickPanel);
    modeQuickStopButton->setObjectName("FS_ModeQuickStopButton");
    modeQuickStopButton->setCursor(Qt::PointingHandCursor);
    modeQuickStopButton->setFocusPolicy(Qt::NoFocus);
    modeQuickStopButton->setEnabled(false);
    modeQuickLayout->addWidget(modeQuickStopButton);

    modeQuickGroup->addButton(modeQuickManualButton, 0);
    modeQuickGroup->addButton(modeQuickAutoButton, 1);
    modeQuickGroup->addButton(modeQuickControlButton, 2);
    modeQuickGroup->addButton(modeQuickPatrolButton, 3);
    modeQuickManualButton->setChecked(true);
    modeQuickPanel->setVisible(true);
    modeQuickPanel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Add Bottom Bar to VBox
    videoLayout->addWidget(underBar);

    mainLayout->addWidget(videoContainer);
    updateModeQuickPanelGeometry();
    modeQuickPanel->raise();

    connect(underBar, &FullUnderBar::reqZoomIn, this, &FullScreenView::onZoomIn);
    connect(underBar, &FullUnderBar::reqZoomOut, this, &FullScreenView::onZoomOut);
    connect(underBar, &FullUnderBar::reqRectZoom, this, &FullScreenView::onRectZoomToggled);

    connect(underBar, &FullUnderBar::reqResetZoom, this, &FullScreenView::onResetZoom);
    connect(underBar, &FullUnderBar::reqControlMode, this, &FullScreenView::onControlModeToggled);
    connect(modeQuickGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int id) {
        emit robotModeSelectionRequested(id);
    });
    connect(modeQuickStopButton, &QPushButton::clicked, this, &FullScreenView::emergencyStopRequested);

    // [New] Record Connection
    connect(underBar, &FullUnderBar::reqRecord, [this](bool start){
        if (currentChannelId >= 0) {
            emit recordRequested(currentChannelId, start);
        }
    });

    // [New] Playback Control Connections
    connect(underBar, &FullUnderBar::reqPlayPause, [this](){
        if(videoWidget->isPlaying()) videoWidget->pause();
        else videoWidget->resume();
    });

    connect(underBar, &FullUnderBar::reqSkipBackward, [this](){
        videoWidget->seekRelative(-5000); // -5s
    });

    connect(underBar, &FullUnderBar::reqSkipForward, [this](){
        videoWidget->seekRelative(5000); // +5s
    });

    connect(underBar, &FullUnderBar::reqSeek, [this](qint64 val){
        // Slider value 0-1000 mapping to Duration
        qint64 duration = videoWidget->getDuration();
        if (duration > 0) {
            qint64 target = (val * duration) / 1000;
            videoWidget->seek(target);
        }
    });

    // Update UI from VideoWidget
    // Update UI from VideoWidget (Connect Both)
    // Live Widget (limited signals)
     connect(liveWidget, &VideoWidget::playbackStateChanged, [this](bool playing){
        if (videoWidget == liveWidget) underBar->setPlaying(playing);
    });

    // Recorded Widget
    connect(recordedWidget, &VideoWidget::positionChanged, [this](qint64 pos){
        if (videoWidget == recordedWidget) {
            qint64 dur = recordedWidget->getDuration();
            underBar->updateTime(pos, dur);
        }
    });
    
    connect(recordedWidget, &VideoWidget::playbackStateChanged, [this](bool playing){
        if (videoWidget == recordedWidget) underBar->setPlaying(playing);
    });
    


    connect(videoWidget, &VideoWidget::durationChanged, [this](qint64 dur){
         // Time update handles duration too
    });

    rubberBand = nullptr;
    GoalOverlay::ArrowStyle overlayStyle;
    overlayStyle.penWidth = 4.0;
    overlayStyle.arrowSize = 15.0;
    controlOverlay = new GoalArrowOverlayWidget(overlayStyle, this);
    controlOverlay->hide();

    // [New] 글로벌 애플리케이션 포커스 상태 변경 감지
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive) {
            if (controlOverlay) controlOverlay->hide();
        } else {
            if (controlOverlay && this->isVisible() &&
                (currentMode == ControlMode || controlOverlay->hasCommittedArrow())) {
                controlOverlay->show();
            }
        }
    });

    syncTimer = new QTimer(this);
    connect(syncTimer, &QTimer::timeout, this, &FullScreenView::syncOverlayPosition);
    syncTimer->start(16); // ~60fps sync rate

    isSettingDirection = false;
    isDrawing = false;
    isPanning = false;
    currentMode = Normal;
}

void FullScreenView::setControlModeAvailable(bool available)
{
    m_controlModeAvailable = available;
    const bool canControlVideo = canControlCurrentVideo();
    underBar->setControlModeAvailable(canControlVideo);
    if ((!m_controlModeAvailable || !canControlVideo) && currentMode == ControlMode) {
        setMode(Normal);
    }
}

void FullScreenView::setControlModeChecked(bool checked)
{
    if (underBar) {
        underBar->setControlModeChecked(checked && canControlCurrentVideo());
    }

    if (checked) {
        if (m_controlModeAvailable && canControlCurrentVideo()) {
            setMode(ControlMode);
        }
        return;
    }

    if (currentMode == ControlMode) {
        if (!hasActiveZoom()) {
            setMode(Normal);
        } else {
            setMode(Zoomed);
        }
    }
}

void FullScreenView::setRobotModeSelection(int mode)
{
    if (!modeQuickGroup) {
        return;
    }

    if (QAbstractButton *button = modeQuickGroup->button(mode)) {
        const QSignalBlocker blocker(modeQuickGroup);
        button->setChecked(true);
    }

    if (modeQuickStopButton) {
        modeQuickStopButton->setEnabled(mode >= 0 && mode <= 3);
    }
}

void FullScreenView::updateModeQuickPanelGeometry()
{
    if (!modeQuickPanel) {
        return;
    }

    QWidget *parent = modeQuickPanel->parentWidget();
    if (!parent) {
        return;
    }

    const QSize panelSize = modeQuickPanel->sizeHint().expandedTo(modeQuickPanel->minimumSizeHint());
    const int leftMargin = 18;
    const int bottomMargin = (underBar ? underBar->height() : 0) + 10;
    const int x = leftMargin;
    const int y = qMax(0, parent->height() - bottomMargin - panelSize.height());

    modeQuickPanel->setGeometry(x, y, panelSize.width(), panelSize.height());
    modeQuickPanel->raise();
}

void FullScreenView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateModeQuickPanelGeometry();
}

void FullScreenView::setVideoGoalOverlay(int channelIndex, const QPointF &normalizedStart, const QPointF &normalizedEnd)
{
    const bool preserveLocalCache = m_preserveVideoGoalLocalCacheOnNextSet
                                    && m_hasVideoGoalOverlay
                                    && m_videoGoalOverlayChannelIndex == channelIndex
                                    && GoalOverlay::samePoint(m_videoGoalStartNormalized, normalizedStart, GoalOverlay::kPointCacheEpsilon)
                                    && GoalOverlay::samePoint(m_videoGoalEndNormalized, normalizedEnd, GoalOverlay::kPointCacheEpsilon);
    m_preserveVideoGoalLocalCacheOnNextSet = false;
    m_hasVideoGoalOverlay = true;
    m_videoGoalOverlayChannelIndex = channelIndex;
    m_videoGoalStartNormalized = normalizedStart;
    m_videoGoalEndNormalized = normalizedEnd;
    if (!preserveLocalCache) {
        m_hasVideoGoalLocalCache = false;
        m_videoGoalStartLocal = QPointF();
        m_videoGoalEndLocal = QPointF();
        m_videoGoalDisplayRect = QRectF();
        m_videoGoalCropRect = QRectF();
    }
    updateCommittedGoalOverlay();
}

void FullScreenView::clearVideoGoalOverlay()
{
    m_hasVideoGoalOverlay = false;
    m_videoGoalOverlayChannelIndex = -1;
    m_videoGoalStartNormalized = QPointF();
    m_videoGoalEndNormalized = QPointF();
    m_hasVideoGoalLocalCache = false;
    m_videoGoalStartLocal = QPointF();
    m_videoGoalEndLocal = QPointF();
    m_videoGoalDisplayRect = QRectF();
    m_videoGoalCropRect = QRectF();
    m_preserveVideoGoalLocalCacheOnNextSet = false;
    updateCommittedGoalOverlay();
}

void FullScreenView::setMapGeometry(const QPointF &origin, int widthCells, int heightCells, double resolution)
{
    m_mapOrigin = origin;
    m_mapWidthCells = widthCells;
    m_mapHeightCells = heightCells;
    m_mapResolution = resolution;
}

void FullScreenView::clearGoalOverlay()
{
    isSettingDirection = false;
    goalStartPos = QPointF();
    m_hasVideoGoalOverlay = false;
    m_videoGoalOverlayChannelIndex = -1;
    m_videoGoalStartNormalized = QPointF();
    m_videoGoalEndNormalized = QPointF();
    m_hasVideoGoalLocalCache = false;
    m_videoGoalStartLocal = QPointF();
    m_videoGoalEndLocal = QPointF();
    m_videoGoalDisplayRect = QRectF();
    m_videoGoalCropRect = QRectF();
    m_preserveVideoGoalLocalCacheOnNextSet = false;
    if (controlOverlay) {
        controlOverlay->clearAll();
    }
}

void FullScreenView::requestCloseView()
{
    QTimer::singleShot(0, this, [this]() {
        emit closeRequested();
    });
}

void FullScreenView::play(const QString &url, int index)
{
    if (syncTimer && !syncTimer->isActive()) {
        syncTimer->start(16);
    }

    zoomHistory.clear();
    setMode(Normal);
    clearGoalOverlay();
    setControlModeChecked(false);
    
    currentChannelId = index; // [New] Store index

    QString name = getChannelName(index);
    titleLabel->setText(name); 

    // [Mod] Show/Hide Playback Controls & Switch Widget
    bool isFile = QFileInfo::exists(url);
    underBar->setMode(isFile);
    if (modeQuickPanel) {
        modeQuickPanel->setVisible(!isFile);
        updateModeQuickPanelGeometry();
    }
    
    if(isFile) {
         titleLabel->setText(QString("Playback: %1").arg(QFileInfo(url).fileName()));
         liveBadge->setVisible(false);
         
         // Switch to Recorded Widget
         videoWidget = recordedWidget;
         videoStack->setCurrentWidget(recordedWidget);
         
         // Fix connection for generic play/pause to target correct widget?
         // Actually the lambda uses `videoWidget` pointer which is updated here!
         // But lambda captures `this`, so `videoWidget` access inside lambda is dynamic?
         // No, `[this](){ videoWidget->... }` accesses `this->videoWidget`.
         // So updating `videoWidget` member variable is sufficient!
         
    } else {
         liveBadge->setVisible(true);
         
         // Switch to Live Widget
         videoWidget = liveWidget;
         videoStack->setCurrentWidget(liveWidget);
    }

    setControlModeAvailable(m_controlModeAvailable);

    const int playbackLatency = isFile ? 0 : 200;
    videoWidget->playUrl(url, playbackLatency);
}

void FullScreenView::stop()
{
    if (syncTimer && syncTimer->isActive()) {
        syncTimer->stop();
    }
    if (rubberBand) {
        rubberBand->hide();
    }
    if (controlOverlay) {
        controlOverlay->clearAll();
        controlOverlay->hide();
    }

    if (liveWidget) {
        liveWidget->stop();
    }
    if (recordedWidget) {
        recordedWidget->stop();
    }
    currentChannelId = -1;
    videoWidget = liveWidget;
    // titleLabel->hide(); // Preserved part of layout
    zoomHistory.clear();
    setMode(Normal);
    underBar->setControlModeAvailable(false);
    setControlModeChecked(false);
    clearGoalOverlay();
}

void FullScreenView::setMode(Mode mode)
{
    currentMode = mode;
    isDrawing = false;
    isPanning = false;
    isSettingDirection = false;
    if(rubberBand) rubberBand->hide();
    if(controlOverlay) {
        controlOverlay->setActive(mode == ControlMode);
        if (!controlOverlay->hasCommittedArrow()) {
            controlOverlay->hide();
        }
    }

    switch(mode) {
    case Normal:
        videoWidget->setCursor(Qt::ArrowCursor);
        videoWidget->resetCrop();
        underBar->setRectButtonMode(0);
        // btnClose->show(); // Always visible 
        zoomHistory.clear();
        break;

    case Drawing:
        videoWidget->setCursor(Qt::CrossCursor);
        underBar->setRectButtonMode(1);
        videoWidget->setMouseTracking(false);
        // btnClose->show();
        break;

    case ControlMode:
        videoWidget->setCursor(Qt::CrossCursor);
        videoWidget->setMouseTracking(true); // Hover 이벤트 받기 위함
        break;

    case Zoomed:
        videoWidget->setCursor(Qt::OpenHandCursor);
        underBar->setRectButtonMode(2);
        videoWidget->setMouseTracking(false);
        // btnClose->hide(); // Can keep visible if overlay
        break;
    }
}

void FullScreenView::onResetZoom() {
    clearGoalOverlay();
    emit videoGoalOverlayClearRequested();
    zoomHistory.clear();
    setMode(Normal);
}

void FullScreenView::onZoomIn() {
    QRectF current = videoWidget->getCurrentCrop();
    zoomHistory.push(current);

    double newW = current.width() * 0.5;
    double newH = current.height() * 0.5;
    double newX = current.x() + (current.width() - newW) / 2.0;
    double newY = current.y() + (current.height() - newH) / 2.0;

    videoWidget->applyCrop(QRectF(newX, newY, newW, newH));

    currentMode = Zoomed;
    videoWidget->setCursor(Qt::OpenHandCursor);
    underBar->setRectButtonMode(2);
    // btnClose->hide();
}

void FullScreenView::onZoomOut() {
    // [수정] 현재 보고 있는 영역 기준으로 줌 아웃 (팬 이동 반영)
    QRectF current = videoWidget->getCurrentCrop();

    // 이미 전체화면이거나 거의 전체화면이면 리셋
    if (current.width() >= 0.99 || current.height() >= 0.99) {
        zoomHistory.clear();
        setMode(Normal);
        return;
    }

    // 이전보다 2배 크기로 확대 (ZoomIn이 0.5배였으므로 역연산)
    double newW = current.width() * 2.0;
    double newH = current.height() * 2.0;

    // 줌 아웃 결과가 전체화면보다 크거나 같으면 전체화면으로 복귀
    if (newW >= 0.99 || newH >= 0.99) {
        zoomHistory.clear();
        setMode(Normal);
        return;
    }

    // 현재 화면의 중심점 유지
    double cx = current.center().x();
    double cy = current.center().y();

    double newX = cx - (newW / 2.0);
    double newY = cy - (newH / 2.0);

    // 화면 벗어나지 않도록 보정 (Clamping)
    newX = qBound(0.0, newX, 1.0 - newW);
    newY = qBound(0.0, newY, 1.0 - newH);

    videoWidget->applyCrop(QRectF(newX, newY, newW, newH));

    // 히스토리 관리 (깊이 유지를 위해 pop, 값은 사용 안함)
    if (!zoomHistory.isEmpty()) {
        zoomHistory.pop();
    }
    
    // 모드 유지 (Zoomed)
    currentMode = Zoomed;
    videoWidget->setCursor(Qt::OpenHandCursor);
    underBar->setRectButtonMode(2);
}

void FullScreenView::onRectZoomToggled(bool checked) {
    if (currentMode == Drawing) {
        if (!hasActiveZoom()) setMode(Normal);
        else {
            currentMode = Zoomed;
            videoWidget->setCursor(Qt::OpenHandCursor);
            underBar->setRectButtonMode(2);
            isDrawing = false;
        }
    } else {
        setMode(Drawing);
    }
}

void FullScreenView::onControlModeToggled(bool checked) {
    if (!canControlCurrentVideo()) {
        underBar->setControlModeAvailable(false);
        underBar->setControlModeChecked(false);
        return;
    }

    emit controlModeRequested(checked);

    if (checked) {
        if (m_controlModeAvailable) {
            setMode(ControlMode);
        } else {
            underBar->setControlModeChecked(false);
        }
    } else {
        if (!hasActiveZoom()) setMode(Normal);
        else setMode(Zoomed);
    }
}

QString FullScreenView::getChannelName(int index) {
    if (index < 4) return QString("CCTV Camera %1").arg(index + 1);
    return "Robot Camera";
}

bool FullScreenView::canControlCurrentVideo() const
{
    return (videoWidget == liveWidget) && currentChannelId >= 0 && currentChannelId < 4;
}

bool FullScreenView::hasActiveZoom() const
{
    if (!videoWidget) {
        return false;
    }

    const QRectF currentCrop = videoWidget->getCurrentCrop();
    return currentCrop.width() < 0.99 || currentCrop.height() < 0.99;
}

void FullScreenView::applyWheelZoom(const QPointF &widgetPoint, double steps)
{
    if (!videoWidget || qFuzzyIsNull(steps)) {
        return;
    }

    const QRectF currentCrop = videoWidget->getCurrentCrop();
    if (!currentCrop.isValid() || currentCrop.width() <= 0.0 || currentCrop.height() <= 0.0) {
        return;
    }

    bool ok = false;
    QPointF anchor = videoWidget->widgetPointToVideoNormalized(widgetPoint, &ok);
    if (!ok) {
        anchor = currentCrop.center();
    }

    const double zoomScale = std::pow(1.0 / kWheelZoomStepFactor, steps);
    const double newW = qBound(kMinCropExtent, currentCrop.width() * zoomScale, 1.0);
    const double newH = qBound(kMinCropExtent, currentCrop.height() * zoomScale, 1.0);

    if (newW >= 0.99 || newH >= 0.99) {
        setMode(Normal);
        return;
    }

    const double anchorRatioX = currentCrop.width() > 0.0
        ? (anchor.x() - currentCrop.x()) / currentCrop.width()
        : 0.5;
    const double anchorRatioY = currentCrop.height() > 0.0
        ? (anchor.y() - currentCrop.y()) / currentCrop.height()
        : 0.5;

    double newX = anchor.x() - (anchorRatioX * newW);
    double newY = anchor.y() - (anchorRatioY * newH);
    newX = qBound(0.0, newX, 1.0 - newW);
    newY = qBound(0.0, newY, 1.0 - newH);

    videoWidget->applyCrop(QRectF(newX, newY, newW, newH));
    currentMode = Zoomed;
    videoWidget->setCursor(Qt::OpenHandCursor);
    underBar->setRectButtonMode(2);
}

bool FullScreenView::eventFilter(QObject *obj, QEvent *event)
{
    if (obj != liveWidget && obj != recordedWidget) {
        return QWidget::eventFilter(obj, event);
    }

    if (obj != videoWidget) {
        return QWidget::eventFilter(obj, event);
    }

    if (event->type() == QEvent::Resize) {
        if (controlOverlay && controlOverlay->isVisible() && isVisible()) {
            controlOverlay->syncToWidget(videoWidget);
            controlOverlay->setClipRect(videoWidget->getVideoDisplayRect());
        }
        updateCommittedGoalOverlay();
        return QWidget::eventFilter(obj, event);
    }

    if (event->type() == QEvent::Wheel && currentMode != Drawing && currentMode != ControlMode) {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);
        if (wheelEvent->angleDelta().y() == 0) {
            return QWidget::eventFilter(obj, event);
        }

        if (wheelEvent->angleDelta().y() < 0 && !hasActiveZoom()) {
            return QWidget::eventFilter(obj, event);
        }

        applyWheelZoom(wheelEvent->position(),
                       static_cast<double>(wheelEvent->angleDelta().y()) / 120.0);
        wheelEvent->accept();
        return true;
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        if (currentMode != Normal) {
            return true;
        }
        requestCloseView();
        return true;
    }

    if (currentMode == Drawing) {
        if (event->type() != QEvent::MouseButtonPress
            && event->type() != QEvent::MouseMove
            && event->type() != QEvent::MouseButtonRelease) {
            return QWidget::eventFilter(obj, event);
        }

        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (event->type() == QEvent::MouseButtonPress && me->button() == Qt::LeftButton) {
            if (!rubberBand) {
                rubberBand = new SimpleRubberBand(nullptr);
            }
            originPoint = me->globalPos();
            rubberBand->setGeometry(QRect(originPoint, QSize()));
            rubberBand->show();
            isDrawing = true;
            return true;
        }

        if (event->type() == QEvent::MouseMove && isDrawing) {
            if (rubberBand) {
                rubberBand->setGeometry(QRect(originPoint, me->globalPos()).normalized());
            }
            return true;
        }

        if (event->type() == QEvent::MouseButtonRelease && isDrawing) {
            isDrawing = false;
            if (!rubberBand) {
                return true;
            }

            const QRect globalRect = rubberBand->geometry();
            rubberBand->hide();
            if (globalRect.width() < 10 || globalRect.height() < 10) {
                return true;
            }

            const QPoint localTopLeft = videoWidget->mapFromGlobal(globalRect.topLeft());
            const QPoint localBottomRight = videoWidget->mapFromGlobal(globalRect.bottomRight());
            const QRect rect(localTopLeft, localBottomRight);
            const QRectF currentCrop = videoWidget->getCurrentCrop();
            const QRectF targetCrop = selectionRectToCropRect(videoWidget, rect);
            if (!targetCrop.isValid() || targetCrop.width() <= 0.0 || targetCrop.height() <= 0.0) {
                return true;
            }

            zoomHistory.push(currentCrop);
            videoWidget->applyCrop(targetCrop);
            currentMode = Zoomed;
            videoWidget->setCursor(Qt::OpenHandCursor);
            underBar->setRectButtonMode(2);
            return true;
        }

        return QWidget::eventFilter(obj, event);
    }

    if (currentMode == Zoomed) {
        if (event->type() == QEvent::Leave && isPanning) {
            isPanning = false;
            videoWidget->setCursor(Qt::OpenHandCursor);
            return true;
        }

        if (event->type() != QEvent::MouseButtonPress
            && event->type() != QEvent::MouseMove
            && event->type() != QEvent::MouseButtonRelease) {
            return QWidget::eventFilter(obj, event);
        }

        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (event->type() == QEvent::MouseButtonPress && me->button() == Qt::LeftButton) {
            isPanning = true;
            lastDragPos = me->pos();
            videoWidget->setCursor(Qt::ClosedHandCursor);
            return true;
        }

        if (event->type() == QEvent::MouseMove && isPanning) {
            const QPoint delta = me->pos() - lastDragPos;
            lastDragPos = me->pos();
            videoWidget->panView(delta.x(), delta.y());
            return true;
        }

        if (event->type() == QEvent::MouseButtonRelease && isPanning) {
            isPanning = false;
            videoWidget->setCursor(Qt::OpenHandCursor);
            return true;
        }

        return QWidget::eventFilter(obj, event);
    }

    if (currentMode == ControlMode) {
        if (!m_controlModeAvailable || currentChannelId < 0 || currentChannelId >= 4 || videoWidget != liveWidget) {
            return true;
        }

        GoalArrowOverlayWidget *overlay = controlOverlay;
        if (event->type() == QEvent::Leave && isSettingDirection) {
            if (overlay) {
                overlay->setPreviewArrow(goalStartPos, goalStartPos);
            }
            return true;
        }

        if (event->type() != QEvent::MouseButtonPress
            && event->type() != QEvent::MouseMove
            && event->type() != QEvent::MouseButtonRelease) {
            return QWidget::eventFilter(obj, event);
        }

        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        const QPointF mousePos = me->position();

        if (event->type() == QEvent::MouseButtonPress && me->button() == Qt::LeftButton) {
            if (!overlay) {
                return true;
            }

            overlay->syncToWidget(videoWidget);
            const QRectF displayRect = videoWidget->getVideoDisplayRect();
            overlay->setClipRect(displayRect);
            bool ok = false;
            videoWidget->widgetPointToVideoNormalized(mousePos, &ok);
            if (!ok) {
                return true;
            }

            emit goalInteractionStarted();
            goalStartPos = mousePos;
            overlay->setActive(true);
            overlay->setPreviewArrow(goalStartPos, goalStartPos);
            overlay->show();
            overlay->raise();
            isSettingDirection = true;
            return true;
        }

        if (event->type() == QEvent::MouseMove && isSettingDirection) {
            if (overlay) {
                const QRectF displayRect = videoWidget->getVideoDisplayRect();
                overlay->setPreviewArrow(goalStartPos, GoalOverlay::clampPointToRect(mousePos, displayRect));
                overlay->setClipRect(displayRect);
            }
            return true;
        }

        if (event->type() == QEvent::MouseButtonRelease && me->button() == Qt::LeftButton && isSettingDirection) {
            const std::optional<GoalOverlay::CommitResult> commit =
                GoalOverlay::buildCommitResult(videoWidget, goalStartPos, mousePos);
            if (!commit.has_value()) {
                isSettingDirection = false;
                overlay->clearPreview();
                return true;
            }

            m_hasVideoGoalOverlay = true;
            m_videoGoalOverlayChannelIndex = currentChannelId;
            m_videoGoalStartNormalized = commit->normalizedStart;
            m_videoGoalEndNormalized = commit->normalizedEnd;
            m_hasVideoGoalLocalCache = true;
            m_videoGoalStartLocal = commit->localStart;
            m_videoGoalEndLocal = commit->localEnd;
            m_videoGoalDisplayRect = commit->displayRect;
            m_videoGoalCropRect = commit->cropRect;
            m_preserveVideoGoalLocalCacheOnNextSet = true;
            overlay->commitArrow(commit->localStart, commit->localEnd);
            emit videoGoalOverlayCommitted(currentChannelId, commit->normalizedStart, commit->normalizedEnd);
            emit goalCommitted();

            if (currentChannelId == 1) {
                qDebug().noquote() << QStringLiteral("[FullScreen] Channel 2 CALIBRATION_CLICK x1=%1 y1=%2 x2=%3 y2=%4")
                                          .arg(commit->normalizedStart.x(), 0, 'f', 4)
                                          .arg(commit->normalizedStart.y(), 0, 'f', 4)
                                          .arg(commit->normalizedEnd.x(), 0, 'f', 4)
                                          .arg(commit->normalizedEnd.y(), 0, 'f', 4);
                emit calibrationClickRequested(currentChannelId,
                                               commit->normalizedStart.x(),
                                               commit->normalizedStart.y(),
                                               commit->normalizedEnd.x(),
                                               commit->normalizedEnd.y());
                isSettingDirection = false;
                return true;
            }

            bool worldOk = false;
            const QPointF worldPoint = quadrantToWorld(commit->normalizedStart, &worldOk);
            if (worldOk) {
                qDebug().noquote() << QStringLiteral("x: %1  y: %2  angle: %3")
                                          .arg(worldPoint.x(), 0, 'f', 3)
                                          .arg(worldPoint.y(), 0, 'f', 3)
                                          .arg(commit->angleRadians, 0, 'f', 3);
                emit reqGoalPose(worldPoint.x(), worldPoint.y(), commit->angleRadians);
            }

            isSettingDirection = false;
            return true;
        }

        return QWidget::eventFilter(obj, event);
    }

    return QWidget::eventFilter(obj, event);

#if 0
    if (obj == videoWidget) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            if (currentMode != Normal) return true;
            emit closeRequested();
            return true;
        }

        if (currentMode == Drawing) {
            if (event->type() != QEvent::MouseButtonPress
                && event->type() != QEvent::MouseMove
                && event->type() != QEvent::MouseButtonRelease) {
                return QWidget::eventFilter(obj, event);
            }
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (event->type() == QEvent::MouseButtonPress && me->button() == Qt::LeftButton) {
                if (!rubberBand) {
                    // 내부 클래스 SimpleRubberBand 사용
                    rubberBand = new SimpleRubberBand(nullptr);
                }
                originPoint = me->globalPos();
                rubberBand->setGeometry(QRect(originPoint, QSize()));
                rubberBand->show();
                isDrawing = true;
                return true;
            }
            else if (event->type() == QEvent::MouseMove && isDrawing) {
                if(rubberBand) {
                    rubberBand->setGeometry(QRect(originPoint, me->globalPos()).normalized());
                }
                return true;
            }
            else if (event->type() == QEvent::MouseButtonRelease && isDrawing) {
                isDrawing = false;
                if(rubberBand) {
                    QRect globalRect = rubberBand->geometry();
                    rubberBand->hide();

                    if (globalRect.width() < 10 || globalRect.height() < 10) return true;

                    QPoint localTopLeft = videoWidget->mapFromGlobal(globalRect.topLeft());
                    QPoint localBottomRight = videoWidget->mapFromGlobal(globalRect.bottomRight());
                    QRect rect(localTopLeft, localBottomRight);

                    QRectF currentCrop = videoWidget->getCurrentCrop();

                    double selX = (double)rect.x() / videoWidget->width();
                    double selY = (double)rect.y() / videoWidget->height();
                    double selW = (double)rect.width() / videoWidget->width();
                    double selH = (double)rect.height() / videoWidget->height();

                    double targetX = currentCrop.x() + (selX * currentCrop.width());
                    double targetY = currentCrop.y() + (selY * currentCrop.height());
                    double targetW = selW * currentCrop.width();
                    double targetH = selH * currentCrop.height();

                    zoomHistory.push(currentCrop);
                    videoWidget->applyCrop(QRectF(targetX, targetY, targetW, targetH));

            currentMode = Zoomed;
            videoWidget->setCursor(Qt::OpenHandCursor);
            underBar->setRectButtonMode(2);
            isDrawing = false;
            // btnClose->hide();
                }
                return true;
            }
        }
        else if (currentMode == Zoomed) {
            QMouseEvent *me = (event->type() == QEvent::MouseButtonPress
                                || event->type() == QEvent::MouseMove
                                || event->type() == QEvent::MouseButtonRelease)
                                   ? static_cast<QMouseEvent*>(event)
                                   : nullptr;
            if (event->type() == QEvent::MouseButtonPress && me->button() == Qt::LeftButton) {
                isPanning = true;
                lastDragPos = me->pos();
                videoWidget->setCursor(Qt::ClosedHandCursor);
                return true;
            }
            else if (event->type() == QEvent::MouseMove && isPanning) {
                QPoint delta = me->pos() - lastDragPos;
                lastDragPos = me->pos();
                videoWidget->panView(delta.x(), delta.y());
                return true;
            }
            else if (event->type() == QEvent::MouseButtonRelease && isPanning) {
                isPanning = false;
                videoWidget->setCursor(Qt::OpenHandCursor);
                return true;
            }
        }
        else if (currentMode == ControlMode) {
            if (!m_controlModeAvailable || currentChannelId < 0 || currentChannelId >= 4 || videoWidget != liveWidget) {
                return true;
            }
            ControlOverlay *overlay = static_cast<ControlOverlay*>(controlOverlay);

            if (event->type() == QEvent::Leave && isSettingDirection) {
                if (overlay) {
                    overlay->endPos = goalStartPos;
                    overlay->update();
                }
                return true;
            }

            if (event->type() != QEvent::MouseButtonPress
                && event->type() != QEvent::MouseMove
                && event->type() != QEvent::MouseButtonRelease) {
                return true;
            }

            QMouseEvent *me = (event->type() == QEvent::MouseButtonPress
                                || event->type() == QEvent::MouseMove
                                || event->type() == QEvent::MouseButtonRelease)
                                   ? static_cast<QMouseEvent*>(event)
                                   : nullptr;
            if (!me) {
                return true;
            }
            const QPointF mousePos = me->position();

            if (event->type() == QEvent::MouseButtonPress && me->button() == Qt::LeftButton) {
                if (!overlay) {
                    controlOverlay = new ControlOverlay(this);
                    overlay = static_cast<ControlOverlay*>(controlOverlay);
                }

                const QPoint globalTopLeft = videoWidget->mapToGlobal(QPoint(0, 0));
                controlOverlay->setGeometry(QRect(globalTopLeft, videoWidget->size()));
                overlay->setClipRect(videoWidget->getVideoDisplayRect());
                if (!isSettingDirection) {
                    bool ok = false;
                    const QPointF normalizedStart = videoWidget->widgetPointToVideoNormalized(mousePos, &ok);
                    if (!ok) {
                        return true;
                    }
                    emit goalInteractionStarted();
                    goalStartPos = mousePos;
                    overlay->startPos = goalStartPos;
                    overlay->endPos = goalStartPos;
                    overlay->isDrawingArrow = true;
                    overlay->show();
                    overlay->raise();
                    isSettingDirection = true;
                } else if (videoWidget->width() > 0 && videoWidget->height() > 0) {
                    const QRectF displayRect = videoWidget->getVideoDisplayRect();
                    const qreal clampedX = std::clamp(static_cast<qreal>(mousePos.x()),
                                                      static_cast<qreal>(displayRect.left()),
                                                      static_cast<qreal>(displayRect.right()));
                    const qreal clampedY = std::clamp(static_cast<qreal>(mousePos.y()),
                                                      static_cast<qreal>(displayRect.top()),
                                                      static_cast<qreal>(displayRect.bottom()));
                    const QPointF clampedEnd(clampedX, clampedY);
                    bool ok = false;
                    const QPointF normalizedStart = videoWidget->widgetPointToVideoNormalized(goalStartPos, &ok);
                    if (!ok) {
                        isSettingDirection = false;
                        overlay->clearPreview();
                        return true;
                    }
                    const QPointF normalizedEnd = videoWidget->widgetPointToVideoNormalized(clampedEnd, &ok);
                    if (!ok) {
                        isSettingDirection = false;
                        overlay->clearPreview();
                        return true;
                    }
                    m_hasVideoGoalOverlay = true;
                    m_videoGoalOverlayChannelIndex = currentChannelId;
                    m_videoGoalStartNormalized = normalizedStart;
                    m_videoGoalEndNormalized = normalizedEnd;
                    m_hasVideoGoalLocalCache = true;
                    m_videoGoalStartLocal = goalStartPos;
                    m_videoGoalEndLocal = clampedEnd;
                    m_videoGoalDisplayRect = displayRect;
                    m_videoGoalCropRect = videoWidget->getCurrentCrop();
                    m_preserveVideoGoalLocalCacheOnNextSet = true;
                    overlay->commitArrow(goalStartPos, clampedEnd);
                    emit videoGoalOverlayCommitted(currentChannelId, normalizedStart, normalizedEnd);
                    emit goalCommitted();

                    bool worldOk = false;
                    const QPointF worldPoint = quadrantToWorld(normalizedStart, &worldOk);
                    if (worldOk) {
                        const double theta = std::atan2(goalStartPos.y() - clampedEnd.y(),
                                                        clampedEnd.x() - goalStartPos.x());
                        qDebug().noquote() << QStringLiteral("x: %1  y: %2  angle: %3")
                                                  .arg(worldPoint.x(), 0, 'f', 3)
                                                  .arg(worldPoint.y(), 0, 'f', 3)
                                                  .arg(theta, 0, 'f', 3);
                        emit reqGoalPose(worldPoint.x(), worldPoint.y(), theta);
                    }

                    isSettingDirection = false;
                }
                return true;
            }

            if (event->type() == QEvent::MouseMove && isSettingDirection) {
                if (overlay) {
                    overlay->endPos = mousePos;
                    overlay->setClipRect(videoWidget->getVideoDisplayRect());
                    overlay->update();
                }
                return true;
            }

            if (event->type() == QEvent::MouseButtonRelease && me->button() == Qt::LeftButton) {
                return true;
            }
            return true;
            if (event->type() == QEvent::MouseButtonPress && me->button() == Qt::LeftButton) {
                if (!controlOverlay) {
                    controlOverlay = new ControlOverlay(this); // 독립된 윈도우
                }
                
                // [수정] 탑레벨 윈도우이므로 global 좌표 기준으로 Geometry 설정
                QPoint globalTopLeft = videoWidget->mapToGlobal(QPoint(0, 0));
                controlOverlay->setGeometry(QRect(globalTopLeft, videoWidget->size()));
                if (isSettingDirection) {
                    return true;
                }

                if (!isSettingDirection) {
                    // First click: origin
                    // 탑레벨 화면의 좌상단 기준 로컬 좌표계와 일치하므로 me->pos()를 그대로 사용해도 무방합니다.
                    // (단, ControlOverlay가 videoWidget 사이즈와 정확히 일치하므로)
                    goalStartPos = me->pos(); 
                    
                    QPoint globalTopLeft = videoWidget->mapToGlobal(QPoint(0, 0));
                    controlOverlay->setGeometry(QRect(globalTopLeft, videoWidget->size()));
                    
                    ControlOverlay* overlay = static_cast<ControlOverlay*>(controlOverlay);
                    overlay->startPos = goalStartPos;
                    overlay->endPos = overlay->startPos;
                    overlay->isDrawingArrow = true;
                    overlay->show();
                    overlay->raise(); // 다른 자식들 위로
                    
                    videoWidget->grabMouse(); // Grab mouse to ensure we receive MouseMove without button pressed
                    
                    isSettingDirection = true;
                } else {
                    // Second click: finish
                    QPoint endPos = me->pos(); // 로컬 좌표 사용
                    ControlOverlay* overlay = static_cast<ControlOverlay*>(controlOverlay);
                    
                    // calculate theta
                    double dy = overlay->startPos.y() - endPos.y(); // Qt Y is inverted relative to standard Cartesian
                    double dx = endPos.x() - overlay->startPos.x();
                    double theta = std::atan2(dy, dx);
                    if (theta < 0) theta += 2 * M_PI;
                    
                    bool ok = false;
                    const QPointF normalizedStart = videoWidget->widgetPointToVideoNormalized(overlay->startPos, &ok);
                    const QPointF worldPoint = ok ? quadrantToWorld(normalizedStart, &ok) : QPointF();
                    if (ok) {
                        overlay->hasCommittedArrow = true;
                        qDebug().noquote() << QStringLiteral("x: %1  y: %2  angle: %3")
                                                  .arg(worldPoint.x(), 0, 'f', 3)
                                                  .arg(worldPoint.y(), 0, 'f', 3)
                                                  .arg(theta, 0, 'f', 3);
                        emit reqGoalPose(worldPoint.x(), worldPoint.y(), theta);
                    }
                    
                    // 화살표를 유지하기 위해 hide()를 호출하지 않고 방향 설정 상태만 종료합니다.
                    videoWidget->releaseMouse();
                    isSettingDirection = false;
                }
                return true;
            }
            else if (event->type() == QEvent::MouseMove && isSettingDirection) {
                if (controlOverlay) {
                    ControlOverlay* overlay = static_cast<ControlOverlay*>(controlOverlay);
                    overlay->endPos = me->pos(); // 로컬 좌표 사용
                    overlay->update();
                }
                return true;
            }
        }
    }
    else if (obj == videoWidget && event->type() == QEvent::Resize) {
        if (controlOverlay && controlOverlay->isVisible() && isVisible()) {
            QPoint globalTopLeft = videoWidget->mapToGlobal(QPoint(0, 0));
            controlOverlay->setGeometry(QRect(globalTopLeft, videoWidget->size()));
            static_cast<ControlOverlay *>(controlOverlay)->setClipRect(videoWidget->getVideoDisplayRect());
        }
        updateCommittedGoalOverlay();
    }
    return QWidget::eventFilter(obj, event);
#endif
}

void FullScreenView::syncOverlayPosition() {
    updateCommittedGoalOverlay();
    if (controlOverlay && controlOverlay->isVisible() && this->isVisible() && videoWidget) {
        controlOverlay->syncToWidget(videoWidget);
        controlOverlay->setClipRect(videoWidget->getVideoDisplayRect());
    }
}

void FullScreenView::updateCommittedGoalOverlay()
{
    if (!controlOverlay || !videoWidget || isSettingDirection) {
        return;
    }

    const bool canShowSharedOverlay = this->isVisible()
                                      && (videoWidget == liveWidget)
                                      && currentChannelId >= 0
                                      && currentChannelId < 4
                                      && m_hasVideoGoalOverlay
                                      && m_videoGoalOverlayChannelIndex == currentChannelId
                                      && videoWidget->width() > 0
                                      && videoWidget->height() > 0;

    if (!canShowSharedOverlay) {
        controlOverlay->setCommittedArrow(QPointF(), QPointF(), false);
        return;
    }

    const QRectF displayRect = videoWidget->getVideoDisplayRect();
    const QRectF cropRect = videoWidget->getCurrentCrop();
    controlOverlay->syncToWidget(videoWidget);
    controlOverlay->setClipRect(displayRect);
    if (m_hasVideoGoalLocalCache
        && GoalOverlay::sameRect(cropRect, m_videoGoalCropRect, GoalOverlay::kCropRectCacheEpsilon)
        && GoalOverlay::sameRect(displayRect, m_videoGoalDisplayRect, GoalOverlay::kDisplayRectCacheEpsilon)) {
        controlOverlay->setCommittedArrow(m_videoGoalStartLocal, m_videoGoalEndLocal, true);
        return;
    }

    const std::optional<GoalOverlay::ProjectionResult> projection =
        GoalOverlay::projectCommittedGoal(videoWidget,
                                          m_videoGoalStartNormalized,
                                          m_videoGoalEndNormalized);
    if (!projection.has_value()) {
        controlOverlay->setCommittedArrow(QPointF(), QPointF(), false);
        return;
    }

    m_hasVideoGoalLocalCache = true;
    m_videoGoalStartLocal = projection->localStart;
    m_videoGoalEndLocal = projection->localEnd;
    m_videoGoalDisplayRect = projection->displayRect;
    m_videoGoalCropRect = projection->cropRect;
    controlOverlay->setCommittedArrow(projection->localStart, projection->localEnd, true);
}

QPointF FullScreenView::widgetPointToVideoNormalized(const QPointF &widgetPoint) const
{
    if (!videoWidget) {
        return QPointF();
    }

    bool ok = false;
    const QPointF normalized = videoWidget->widgetPointToVideoNormalized(widgetPoint, &ok);
    return ok ? normalized : QPointF();
}

QPointF FullScreenView::videoNormalizedToWidgetPoint(const QPointF &normalizedPoint) const
{
    return videoWidget ? videoWidget->videoNormalizedToWidgetPoint(normalizedPoint) : QPointF();
}

QPointF FullScreenView::quadrantToWorld(const QPointF &normalizedPoint, bool *ok) const
{
    if (ok) {
        *ok = false;
    }

    if (currentChannelId < 0 || currentChannelId >= 4 || m_mapWidthCells <= 0 || m_mapHeightCells <= 0 || m_mapResolution <= 0.0) {
        return QPointF();
    }

    const double fullWidth = m_mapWidthCells * m_mapResolution;
    const double fullHeight = m_mapHeightCells * m_mapResolution;
    const double halfWidth = fullWidth * 0.5;
    const double halfHeight = fullHeight * 0.5;
    const double clampedX = qBound(0.0, normalizedPoint.x(), 1.0);
    const double clampedY = qBound(0.0, normalizedPoint.y(), 1.0);
    const bool isRight = (currentChannelId == 1 || currentChannelId == 3);
    const bool isBottom = (currentChannelId == 2 || currentChannelId == 3);

    const double quadrantMinX = m_mapOrigin.x() + (isRight ? halfWidth : 0.0);
    const double quadrantMinY = m_mapOrigin.y() + (isBottom ? 0.0 : halfHeight);
    const double worldX = quadrantMinX + (clampedX * halfWidth);
    const double worldY = quadrantMinY + ((1.0 - clampedY) * halfHeight);

    if (ok) {
        *ok = true;
    }
    return QPointF(worldX, worldY);
}

