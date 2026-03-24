#ifndef SLAMMAPWIDGET_H
#define SLAMMAPWIDGET_H

#include <QWidget>
#include <QEvent>
#include <QImage>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QWheelEvent>

class SlamMapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SlamMapWidget(QWidget *parent = nullptr);

    void setMapData(const QJsonObject &data);
    void setOdomData(const QJsonObject &data);
    void setPathData(const QJsonObject &data);
    void setGoalTargetingEnabled(bool enabled);
    void setPatrolPlanningEnabled(bool enabled);
    void setPatrolAddPointMode(bool enabled);
    bool hasMapData() const;
    QPointF mapOrigin() const;
    double mapResolution() const;
    int mapWidthCells() const;
    int mapHeightCells() const;
    QVector<QPointF> patrolPoints() const;
    void clearGoalOverlay();
    void clearPathOverlay();
    void clearPatrolOverlay();

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

signals:
    void goalRequested(double x, double y, double yaw);
    void goalInteractionStarted();
    void goalCommitted();
    void patrolPointsChanged(int count);

private:
    enum class ProjectionMode {
        Standard,
        NoRotation,
        Raw
    };

    QPointF worldToImage(const QPointF &worldPoint) const;
    QPointF worldToImage(const QPointF &worldPoint, ProjectionMode mode) const;
    QPointF widgetToImage(const QPointF &widgetPoint, bool *ok = nullptr,
                          bool requireInsideMapRect = true) const;
    QPointF imageToWidget(const QPointF &imagePoint) const;
    QPointF imageToWorld(const QPointF &imagePoint, ProjectionMode mode) const;
    QPointF widgetToWorld(const QPointF &widgetPoint, bool *ok = nullptr,
                          bool requireInsideMapRect = true) const;
    QRectF fittedMapRect() const;
    QRectF visibleImageRect() const;
    void resetView();
    void setZoomFactor(double zoomFactor, const QPointF &anchorWidgetPos = QPointF());
    void clampViewCenter();
    void updateInteractionCursor();
    bool canPan() const;
    ProjectionMode bestProjectionModeForWorldPoint(const QPointF &worldPoint) const;
    QString projectionModeLabel(ProjectionMode mode) const;

    QImage m_mapImage;
    int m_mapWidth = 0;
    int m_mapHeight = 0;
    double m_resolution = 0.05;
    QPointF m_origin;
    double m_originYaw = 0.0;
    QString m_mapFrameId = QString("map");

    bool m_hasMap = false;
    bool m_hasOdom = false;
    QPointF m_robotPosition;
    double m_robotTheta = 0.0;
    QVector<QPointF> m_pathPoints;
    QString m_lastRejectedOdomFrameId;
    QString m_lastRejectedPathFrameId;

    bool m_goalTargetingEnabled = false;
    bool m_isSettingGoalDirection = false;
    QPointF m_goalStartWidgetPos;
    QPointF m_goalEndWidgetPos;
    bool m_hasCommittedGoalArrow = false;
    QPointF m_committedGoalStartWorldPos;
    QPointF m_committedGoalEndWorldPos;
    bool m_patrolPlanningEnabled = false;
    bool m_patrolAddPointMode = false;
    QVector<QPointF> m_patrolPoints;

    double m_zoomFactor = 1.0;
    QPointF m_viewCenterImage;
    bool m_isPanning = false;
    QPointF m_lastPanWidgetPos;
    ProjectionMode m_projectionMode = ProjectionMode::Standard;
};

#endif // SLAMMAPWIDGET_H
