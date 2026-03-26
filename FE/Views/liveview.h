#ifndef LIVEVIEW_H
#define LIVEVIEW_H

#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QEvent>
#include <QShowEvent>
#include <QJsonObject>
#include "videocard.h"
#include "slammapwidget.h"

class LiveView : public QWidget
{
    Q_OBJECT
public:
    explicit LiveView(QWidget *parent = nullptr);
    bool isRcCarCameraFocused() const;
    void setChannelVisible(int index, bool visible);
    void stopAll();
    void clearGoalOverlays();
    void clearPathOverlay();
    void clearPatrolOverlay();
    QVector<QPointF> patrolPoints() const;
    void setVideoGoalOverlay(int channelIndex, const QPointF &normalizedStart, const QPointF &normalizedEnd);
    void clearVideoGoalOverlay();

signals:
    void requestFullScreen(int index, QString url);
    void recordCommandRequested(int channelId, bool start);
    void goalPoseRequested(double x, double y, double yaw);
    void calibrationClickRequested(int channelIndex, double x1, double y1, double x2, double y2);
    void goalInteractionStarted();
    void goalCommitted();
    void patrolPointsChanged(int count);
    void videoGoalOverlayCommitted(int channelIndex, QPointF normalizedStart, QPointF normalizedEnd);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override; // [Fix] Focus on click

public slots:
    void updateMap(const QJsonObject &data);
    void updateOdom(const QJsonObject &data);
    void updatePath(const QJsonObject &data);
    void setGoalTargetingEnabled(bool enabled);
    void setPatrolPlanningEnabled(bool enabled);
    void setPatrolAddPointMode(bool enabled);

private:
    QGridLayout *gridLayout;
    VideoCard *cctvWidgets[4]; 
    QWidget *rightPanel;       
    
    // Right Panel Widgets
    VideoCard *rcCarCamWidget;
    SlamMapWidget *slamMapWidget;

    bool streamStarted;

    QStringList lowQualityUrls;
    QStringList highQualityUrls;
    QPointF m_mapOrigin;
    double m_mapResolution = 0.0;
    int m_mapWidthCells = 0;
    int m_mapHeightCells = 0;
    bool m_goalTargetingEnabled = false;

    void initCCTVStreams();
    void updateCCTVLayout(); // Dynamic layout update
    QPointF quadrantToWorld(int index, const QPointF &normalizedPoint, bool *ok = nullptr) const;

private slots:
    void refreshStreams();
};

#endif // LIVEVIEW_H
