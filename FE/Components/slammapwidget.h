#ifndef SLAMMAPWIDGET_H
#define SLAMMAPWIDGET_H

#include <QWidget>
#include <QImage>
#include <QJsonObject>
#include <QPointF>
#include <QVector>

class SlamMapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SlamMapWidget(QWidget *parent = nullptr);

    void setMapData(const QJsonObject &data);
    void setOdomData(const QJsonObject &data);
    void setPathData(const QJsonObject &data);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPointF worldToImage(const QPointF &worldPoint) const;
    QRectF fittedMapRect() const;

    QImage m_mapImage;
    int m_mapWidth = 0;
    int m_mapHeight = 0;
    double m_resolution = 0.05;
    QPointF m_origin;

    bool m_hasMap = false;
    bool m_hasOdom = false;
    QPointF m_robotPosition;
    double m_robotTheta = 0.0;
    QVector<QPointF> m_pathPoints;
};

#endif // SLAMMAPWIDGET_H
