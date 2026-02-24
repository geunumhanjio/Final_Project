#ifndef FULLSCREENVIEW_H
#define FULLSCREENVIEW_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QStack>
#include <QStackedWidget> // [New]
// [수정] QRubberBand 대신 QWidget 사용 (커스텀 스타일링을 위해)
// #include <QRubberBand>
#include "videowidget.h"
#include "livevideowidget.h"
#include "recordedvideowidget.h"
#include "full_underbar.h"

class FullScreenView : public QWidget
{
    Q_OBJECT
public:
    explicit FullScreenView(QWidget *parent = nullptr);
    void play(const QString &url, int index);
    void stop();
 // Theme Support

signals:
    void closeRequested();
    void recordRequested(int channelId, bool start); // [New]
    void reqGoalPose(double x, double y, double theta);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onZoomIn();
    void onZoomOut();
    void onRectZoomToggled(bool checked);
    void onResetZoom();
    void onControlModeToggled(bool checked);

private:
    QStackedWidget *videoStack; // [New]
    LiveVideoWidget *liveWidget; // [New]
    RecordedVideoWidget *recordedWidget; // [New]
    
    VideoWidget *videoWidget; // Pointer to active widget
    FullUnderBar *underBar;
    
    // Top Bar Components
    QWidget *topBar;
    QLabel *titleLabel;
    QLabel *liveBadge;
    QPushButton *btnClose;

    // [수정] QRubberBand* -> QWidget* 으로 변경
    QWidget *rubberBand;

    QWidget *controlOverlay;
    bool isSettingDirection;
    QPoint goalStartPos;

    QPoint originPoint;
    bool isDrawing;

    QPoint lastDragPos;
    bool isPanning;

    QStack<QRectF> zoomHistory;
    enum Mode { Normal, Drawing, Zoomed, ControlMode };
    Mode currentMode;

    QString getChannelName(int index);
    void setMode(Mode mode);
    
    int currentChannelId = -1; // [New] Track current channel
};

#endif // FULLSCREENVIEW_H
