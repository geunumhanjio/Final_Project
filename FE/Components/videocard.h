#ifndef VIDEOCARD_H
#define VIDEOCARD_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QPointF>
#include <QTimer>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QEnterEvent>
#else
#include <QEvent>
#endif
#include "videowidget.h"

class GoalArrowOverlayWidget;

class VideoCard : public QWidget
{
    Q_OBJECT
public:
    explicit VideoCard(QWidget *parent = nullptr);
    ~VideoCard();

    // Proxy methods to control the inner VideoWidget
    void playUrl(const QString &url, int latency = 200);
    void stop();
    VideoWidget* videoWidget() const { return m_videoWidget; }

    // UI Setters
    void setChannelName(const QString &name);
    void setChannelStatus(bool active);
    void setStreamInfo(const QString &info);
    void showRecIndicator(bool show);
    void setChannelId(int id) { m_channelId = id; }
    void setGoalTargetingEnabled(bool enabled);
    bool isGoalTargetingEnabled() const;
    void clearGoalOverlay();
    void setCommittedGoalOverlay(const QPointF &normalizedStart, const QPointF &normalizedEnd);

signals:
    void fullScreenRequested();
    void recordRequested(int channelId, bool start);
    void goalRequested(QPointF normalizedStart, double yaw);
    void goalInteractionStarted();
    void goalCommitted();
    void goalOverlayCommitted(QPointF normalizedStart, QPointF normalizedEnd);

private slots:
    void toggleRecord();
    void showSettingsMenu(); // [New] Settings Menu Slot

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    bool eventFilter(QObject *watched, QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void setupUi();
    void updateOverlayLayout();
    void syncGoalOverlayPosition();
    void updateCommittedGoalOverlay();

    // Core Components
    VideoWidget *m_videoWidget;

    // Overlays
    QWidget *m_topOverlay;
    QWidget *m_bottomOverlay; // In-video bottom overlay (e.g. resolution info)
    GoalArrowOverlayWidget *m_goalOverlay;
    
    // Status Bar (Below video)
    QFrame *m_statusBar;
    QLabel *m_statusIcon;
    QLabel *m_channelLabel;
    QLabel *m_muteIcon;

    // UI Elements
    QWidget *m_recBadge;
    QLabel *m_recLabel;
    QPushButton *m_btnFullscreen;
    QPushButton *m_btnSettings;
    QPushButton *m_btnRecord; // [New] Record Button
    QLabel *m_streamInfoLabel;

    bool m_isHovered;
    bool m_isRecording; // [New] Recording State
    int m_channelId;    // [New] Channel ID
    bool m_goalTargetingEnabled = false;
    bool m_isSettingGoalDirection = false;
    QPointF m_goalStartPos;
    QPointF m_goalEndPos;
    bool m_hasCommittedGoalOverlay = false;
    QPointF m_committedGoalStartNormalized;
    QPointF m_committedGoalEndNormalized;
    bool m_hasCommittedGoalLocalCache = false;
    QPointF m_committedGoalStartLocal;
    QPointF m_committedGoalEndLocal;
    QRectF m_committedGoalDisplayRect;
    QRectF m_committedGoalCropRect;
    bool m_preserveLocalCommittedGoalOnNextSet = false;
    QTimer *m_goalOverlaySyncTimer = nullptr;
};

#endif // VIDEOCARD_H
