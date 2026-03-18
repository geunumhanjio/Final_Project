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
    void setControlModeAvailable(bool available);
    void setControlModeChecked(bool checked);
    void setVideoGoalOverlay(int channelIndex, const QPointF &normalizedStart, const QPointF &normalizedEnd);
    void clearVideoGoalOverlay();
    void setMapGeometry(const QPointF &origin, int widthCells, int heightCells, double resolution);
    void clearGoalOverlay();

signals:
    void closeRequested();
    void recordRequested(int channelId, bool start); // [New]
    void controlModeRequested(bool enabled);
    void reqGoalPose(double x, double y, double theta);
    void goalInteractionStarted();
    void goalCommitted();
    void videoGoalOverlayCommitted(int channelIndex, QPointF normalizedStart, QPointF normalizedEnd);
    void videoGoalOverlayClearRequested();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onZoomIn();
    void onZoomOut();
    void onRectZoomToggled(bool checked);
    void onResetZoom();
    void onControlModeToggled(bool checked);

private:
    void requestCloseView();
    QStackedWidget *videoStack; // [New]
    LiveVideoWidget *liveWidget; // [New]
    RecordedVideoWidget *recordedWidget; // [New]
    
    VideoWidget *videoWidget; // Pointer to active widget
    FullUnderBar *underBar;
    
    // Top Bar Components
    QWidget *topBar;
    QLabel *titleLabel;
    QLabel *liveBadge;
    QPushButton *btnSettings; // [New]
    QPushButton *btnClose;

    // [수정] QRubberBand* -> QWidget* 으로 변경
    QWidget *rubberBand;

    QWidget *controlOverlay;
    QTimer *syncTimer; // [New]
    void syncOverlayPosition(); // [New]

    bool isSettingDirection;
    QPointF goalStartPos;

    QPoint originPoint;
    bool isDrawing;

    QPoint lastDragPos;
    bool isPanning;

    QStack<QRectF> zoomHistory;
    enum Mode { Normal, Drawing, Zoomed, ControlMode };
    Mode currentMode;

    QString getChannelName(int index);
    bool canControlCurrentVideo() const;
    void setMode(Mode mode);
    QPointF quadrantToWorld(const QPointF &normalizedPoint, bool *ok = nullptr) const;
    void updateCommittedGoalOverlay();
    QPointF widgetPointToVideoNormalized(const QPointF &widgetPoint) const;
    QPointF videoNormalizedToWidgetPoint(const QPointF &normalizedPoint) const;
    
    int currentChannelId = -1; // [New] Track current channel
    bool m_controlModeAvailable = false;
    QPointF m_mapOrigin;
    int m_mapWidthCells = 0;
    int m_mapHeightCells = 0;
    double m_mapResolution = 0.0;
    bool m_hasVideoGoalOverlay = false;
    int m_videoGoalOverlayChannelIndex = -1;
    QPointF m_videoGoalStartNormalized;
    QPointF m_videoGoalEndNormalized;
    bool m_hasVideoGoalLocalCache = false;
    QPointF m_videoGoalStartLocal;
    QPointF m_videoGoalEndLocal;
    QRectF m_videoGoalDisplayRect;
    QRectF m_videoGoalCropRect;
    bool m_preserveVideoGoalLocalCacheOnNextSet = false;
};

#endif // FULLSCREENVIEW_H
