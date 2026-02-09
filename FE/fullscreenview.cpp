#include "fullscreenview.h"
#include <QDebug>
#include <QCursor>
#include <QPainter>

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
    
    // Use Stacked Layout equivalent by putting everything in cell (0,0) of a Grid
    QGridLayout *videoLayout = new QGridLayout(videoContainer);
    videoLayout->setContentsMargins(0, 0, 0, 0);

    // 1. Video Layer (Bottom)
    videoWidget = new VideoWidget(videoContainer);
    videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    videoWidget->installEventFilter(this);
    videoLayout->addWidget(videoWidget, 0, 0);

    // 2. Top Info Bar (Overlay Top)
    topBar = new QWidget(videoContainer);
    topBar->setFixedHeight(80);
    // Initial Theme (matches updateTheme(true))
    
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(20, 10, 20, 30); // Extra bottom margin for fade
    
    titleLabel = new QLabel("CCTV Camera", topBar);
    // Initial Style set in updateTheme
    
    liveBadge = new QLabel("LIVE", topBar);
    // Initial Style set in updateTheme
    
    btnClose = new QPushButton("✕", topBar);
    btnClose->setFixedSize(36, 36);
    btnClose->setCursor(Qt::PointingHandCursor);
    // Initial Style set in updateTheme

    connect(btnClose, &QPushButton::clicked, this, &FullScreenView::closeRequested);

    topLayout->addWidget(titleLabel);
    topLayout->addSpacing(10);
    topLayout->addWidget(liveBadge);
    topLayout->addStretch();
    topLayout->addWidget(btnClose);

    // Add TopBar to Grid (0,0, AlignTop)
    videoLayout->addWidget(topBar, 0, 0, Qt::AlignTop);

    // 3. Bottom Control Bar (Overlay Bottom)
    underBar = new FullUnderBar(videoContainer);
    
    QWidget *bottomWrapper = new QWidget(videoContainer);
    bottomWrapper->setAttribute(Qt::WA_TranslucentBackground);
    QVBoxLayout *bwLayout = new QVBoxLayout(bottomWrapper);
    bwLayout->setContentsMargins(0, 0, 0, 40); // 40px margin from bottom
    bwLayout->addWidget(underBar, 0, Qt::AlignHCenter);
    videoLayout->addWidget(bottomWrapper, 0, 0, Qt::AlignBottom);

    mainLayout->addWidget(videoContainer);

    connect(underBar, &FullUnderBar::reqZoomIn, this, &FullScreenView::onZoomIn);
    connect(underBar, &FullUnderBar::reqZoomOut, this, &FullScreenView::onZoomOut);
    connect(underBar, &FullUnderBar::reqRectZoom, this, &FullScreenView::onRectZoomToggled);
    connect(underBar, &FullUnderBar::reqResetZoom, this, &FullScreenView::onResetZoom);

    rubberBand = nullptr;
    isDrawing = false;
    isPanning = false;
    currentMode = Normal;
}

void FullScreenView::play(const QString &url, int index)
{
    zoomHistory.clear();
    setMode(Normal);

    QString name = getChannelName(index);
    titleLabel->setText(name); // Label itself has style
    // titleLabel->show(); // Always visible in topBar
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
    if (zoomHistory.isEmpty()) {
        setMode(Normal);
        return;
    }

    QRectF prevRect = zoomHistory.pop();
    videoWidget->applyCrop(prevRect);

    if (prevRect.width() > 0.99 && prevRect.height() > 0.99) {
        setMode(Normal);
    } else {
        currentMode = Zoomed;
        videoWidget->setCursor(Qt::OpenHandCursor);
        underBar->setRectButtonMode(2);
        // btnClose->hide();
    }
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
