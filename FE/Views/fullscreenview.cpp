#include "fullscreenview.h"
#include <QDebug>
#include <QCursor>
#include <QPainter>
#include <QFileInfo>
#include <QMenu>         // [New]
#include <QAction>       // [New]
#include <QCheckBox>     // [New]
#include <QLabel>        // [New]
#include "osdwidget.h"   // [New]
#include <QGuiApplication>
#include <QApplication>

// =============================================================
// [내부 클래스] 직접 그려지는 네모 박스
// =============================================================
class SimpleRubberBand : public QWidget {
public:
    SimpleRubberBand(QWidget* parent = nullptr) : QWidget(parent) {
        // 1. 테두리 없는 탑레벨 투명 윈도우 설정
        setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);

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
class ControlOverlay : public QWidget {
public:
    QPoint startPos;
    QPoint endPos;
    bool isDrawingArrow;

    ControlOverlay(QWidget* parent = nullptr) : QWidget(parent) {
        // [수정] SimpleRubberBand처럼 비디오 위젯 위에 확실히 그려지도록 탑레벨 윈도우 속성 부여
        setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground, true);
        
        isDrawingArrow = false;
    }

protected:
    void paintEvent(QPaintEvent*) override {
        if (startPos.isNull()) return;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

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
};
// =============================================================


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
                
                // Emit signal to trigger stream stats
                int expectedChannel = (currentChannelId < 4) ? (currentChannelId + 1) : 9;
                emit streamStatsRequested(expectedChannel, anyChecked);
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

    connect(btnClose, &QPushButton::clicked, this, &FullScreenView::closeRequested);

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
    
    QWidget *bottomWrapper = new QWidget(videoContainer);
    bottomWrapper->setAttribute(Qt::WA_TranslucentBackground);
    QVBoxLayout *bwLayout = new QVBoxLayout(bottomWrapper);
    bwLayout->setContentsMargins(0, 0, 0, 10); // Margin reduced (40 -> 10)
    bwLayout->addWidget(underBar, 0, Qt::AlignHCenter);
    
    // Add BottomWrapper to VBox
    videoLayout->addWidget(bottomWrapper);

    mainLayout->addWidget(videoContainer);

    connect(underBar, &FullUnderBar::reqZoomIn, this, &FullScreenView::onZoomIn);
    connect(underBar, &FullUnderBar::reqZoomOut, this, &FullScreenView::onZoomOut);
    connect(underBar, &FullUnderBar::reqRectZoom, this, &FullScreenView::onRectZoomToggled);

    connect(underBar, &FullUnderBar::reqResetZoom, this, &FullScreenView::onResetZoom);
    connect(underBar, &FullUnderBar::reqControlMode, this, &FullScreenView::onControlModeToggled);

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
    controlOverlay = new ControlOverlay(this);
    controlOverlay->hide();

    // [New] 글로벌 애플리케이션 포커스 상태 변경 감지
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive) {
            if (controlOverlay) controlOverlay->hide();
        } else {
            if (currentMode == ControlMode && controlOverlay && this->isVisible()) {
                controlOverlay->show();
            }
        }
    });

    // [New] Timer to constantly sync ControlOverlay position when app moves
    syncTimer = new QTimer(this);
    connect(syncTimer, &QTimer::timeout, this, &FullScreenView::syncOverlayPosition);
    syncTimer->start(16); // ~60fps sync rate

    isSettingDirection = false;
    isDrawing = false;
    isPanning = false;
    currentMode = Normal;
}

void FullScreenView::play(const QString &url, int index)
{
    zoomHistory.clear();
    setMode(Normal);
    
    currentChannelId = index; // [New] Store index

    QString name = getChannelName(index);
    titleLabel->setText(name); 

    // [Mod] Show/Hide Playback Controls & Switch Widget
    bool isFile = QFileInfo::exists(url);
    underBar->setMode(isFile);
    
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

    videoWidget->playUrl(url, 0);
}

void FullScreenView::stop()
{
    videoWidget->stop();
    // titleLabel->hide(); // Preserved part of layout
    zoomHistory.clear();
    setMode(Normal);
}

void FullScreenView::setMode(Mode mode)
{
    currentMode = mode;
    isDrawing = false;
    isPanning = false;
    isSettingDirection = false;
    if(rubberBand) rubberBand->hide();
    if(controlOverlay) {
        controlOverlay->hide();
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
        onResetZoom();
        return;
    }

    // 이전보다 2배 크기로 확대 (ZoomIn이 0.5배였으므로 역연산)
    double newW = current.width() * 2.0;
    double newH = current.height() * 2.0;

    // 줌 아웃 결과가 전체화면보다 크거나 같으면 전체화면으로 복귀
    if (newW >= 0.99 || newH >= 0.99) {
        onResetZoom();
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
        if (zoomHistory.isEmpty()) setMode(Normal);
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
    if (checked) {
        setMode(ControlMode);
    } else {
        if (controlOverlay) controlOverlay->hide();
        if (zoomHistory.isEmpty()) setMode(Normal);
        else setMode(Zoomed);
    }
}

QString FullScreenView::getChannelName(int index) {
    if (index < 4) return QString("CCTV Camera %1").arg(index + 1);
    return "Robot Camera";
}

bool FullScreenView::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == videoWidget) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            if (currentMode != Normal) return true;
            emit closeRequested();
            return true;
        }

        if (currentMode == Drawing) {
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
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
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
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (event->type() == QEvent::MouseButtonPress && me->button() == Qt::LeftButton) {
                if (!controlOverlay) {
                    controlOverlay = new ControlOverlay(nullptr); // 독립된 윈도우
                }
                
                // [수정] 탑레벨 윈도우이므로 global 좌표 기준으로 Geometry 설정
                QPoint globalTopLeft = videoWidget->mapToGlobal(QPoint(0, 0));
                controlOverlay->setGeometry(QRect(globalTopLeft, videoWidget->size()));
                
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
                    
                    // placeholder map scale logic (e.g. 0.05m / px, center is (0,0))
                    double cx = videoWidget->width() / 2.0;
                    double cy = videoWidget->height() / 2.0;
                    double x = (overlay->startPos.x() - cx) * 0.05;
                    double y = (cy - overlay->startPos.y()) * 0.05;

                    emit reqGoalPose(x, y, theta);
                    
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
        if (controlOverlay && currentMode == ControlMode && isVisible()) {
            QPoint globalTopLeft = videoWidget->mapToGlobal(QPoint(0, 0));
            controlOverlay->setGeometry(QRect(globalTopLeft, videoWidget->size()));
        }
    }
    return QWidget::eventFilter(obj, event);
}

void FullScreenView::syncOverlayPosition() {
    if (controlOverlay && currentMode == ControlMode && controlOverlay->isVisible() && this->isVisible() && videoWidget) {
        QPoint globalTopLeft = videoWidget->mapToGlobal(QPoint(0, 0));
        QRect currentRect = controlOverlay->geometry();
        QRect newRect(globalTopLeft, videoWidget->size());
        if (currentRect != newRect) {
            controlOverlay->setGeometry(newRect);
        }
    }
}

void FullScreenView::updateStreamStats(int channelId, double fps, double bitrateKbps, double proxyLatencyMs)
{
    int expectedChannel = (currentChannelId < 4) ? (currentChannelId + 1) : 9;
    if (channelId == expectedChannel && videoWidget && videoWidget->getOsdWidget()) {
        OsdWidget *osd = videoWidget->getOsdWidget();
        
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
