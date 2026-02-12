#include "fullscreenview.h"
#include <QDebug>
#include <QCursor>
#include <QPainter>
#include <QFileInfo>

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
    topBar->setFixedHeight(80);
    // Initial Theme (matches updateTheme(true))
    
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(20, 10, 20, 10); // Bottom margin reduced (30->10)
    
    titleLabel = new QLabel("CCTV Camera", topBar);
    
    liveBadge = new QLabel("LIVE", topBar);
    
    btnClose = new QPushButton("✕", topBar);
    btnClose->setFixedSize(36, 36);
    btnClose->setCursor(Qt::PointingHandCursor);

    connect(btnClose, &QPushButton::clicked, this, &FullScreenView::closeRequested);

    topLayout->addWidget(titleLabel);
    topLayout->addSpacing(10);
    topLayout->addWidget(liveBadge);
    topLayout->addStretch();
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
    if(rubberBand) rubberBand->hide();

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
        // btnClose->show();
        break;

    case Zoomed:
        videoWidget->setCursor(Qt::OpenHandCursor);
        underBar->setRectButtonMode(2);
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
        currentMode = Drawing;
        videoWidget->setCursor(Qt::CrossCursor);
        underBar->setRectButtonMode(1);
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
    }
    return QWidget::eventFilter(obj, event);
}

void FullScreenView::updateTheme(bool isDark)
{
    underBar->updateTheme(isDark);

    if (isDark) {
        // Dark Theme Top Bar
        topBar->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(0,0,0,200), stop:1 transparent);");
        
        // Title Text: Yellow with Dark Box
        titleLabel->setStyleSheet("color: #EAB308; font-weight: bold; font-size: 18px; background-color: rgba(15, 23, 42, 0.6); border-radius: 8px; padding: 4px 12px; border: 1px solid rgba(255, 255, 255, 0.1);");
        
        liveBadge->setStyleSheet("color: #EF4444; font-weight: bold; font-size: 11px; border: 1px solid rgba(239, 68, 68, 0.5); background-color: rgba(220, 38, 38, 0.1); padding: 2px 6px; border-radius: 4px;");
        btnClose->setStyleSheet(
            "QPushButton { background-color: rgba(30, 41, 59, 0.5); color: #CBD5E1; border: 1px solid rgba(51, 65, 85, 0.5); border-radius: 8px; font-size: 16px; }"
            "QPushButton:hover { background-color: rgba(220, 38, 38, 0.9); color: white; border-color: #EF4444; }"
        );
    } else {
        // Light Theme Top Bar (White/Orange)
        topBar->setStyleSheet("background: transparent;");
        
        // Title Text: Dark Gray
        titleLabel->setStyleSheet("color: #111827; font-weight: bold; font-size: 18px; background-color: rgba(255,255,255,0.8); border-radius: 8px; padding: 4px 12px; border: 1px solid rgba(255,255,255,0.6);"); 
        
        // Live Badge: Primary (Orange)
        liveBadge->setStyleSheet("color: #F98006; font-weight: bold; font-size: 11px; border: 1px solid rgba(249, 128, 6, 0.3); background-color: rgba(249, 128, 6, 0.1); padding: 2px 6px; border-radius: 4px;");
        
        // Close Button: White Circle look
        btnClose->setStyleSheet(
            "QPushButton { background-color: rgba(255, 255, 255, 0.9); color: #374151; border: 1px solid rgba(255, 255, 255, 0.5); border-radius: 18px; font-size: 16px; }"
            "QPushButton:hover { background-color: #FFFFFF; color: #111827; transform: scale(1.05); }"
        );
    }
}
