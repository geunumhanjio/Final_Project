#include "slammapwidget.h"

#include <cmath>
#include <QJsonArray>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

SlamMapWidget::SlamMapWidget(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(0, 0);
}

void SlamMapWidget::setMapData(const QJsonObject &data)
{
    const QJsonObject info = data.value("info").toObject();
    const int width = info.value("width").toInt();
    const int height = info.value("height").toInt();
    const double resolution = info.value("resolution").toDouble();
    const QJsonObject origin = info.value("origin").toObject();
    const QJsonArray cells = data.value("data").toArray();

    if (width <= 0 || height <= 0 || resolution <= 0.0 || cells.size() < width * height) {
        return;
    }

    QImage mapImage(width, height, QImage::Format_RGB32);
    for (int y = 0; y < height; ++y) {
        QRgb *scanLine = reinterpret_cast<QRgb *>(mapImage.scanLine(height - 1 - y));
        for (int x = 0; x < width; ++x) {
            const int value = cells.at((y * width) + x).toInt();

            QColor color;
            if (value < 0) {
                color = QColor(71, 85, 105);
            } else {
                const int clamped = qBound(0, value, 100);
                const int shade = 255 - ((clamped * 255) / 100);
                color = QColor(shade, shade, shade);
            }

            scanLine[x] = color.rgb();
        }
    }

    m_mapImage = mapImage;
    m_mapWidth = width;
    m_mapHeight = height;
    m_resolution = resolution;
    m_origin = QPointF(origin.value("x").toDouble(), origin.value("y").toDouble());
    m_hasMap = true;
    update();
}

void SlamMapWidget::setOdomData(const QJsonObject &data)
{
    const QJsonObject position = data.value("position").toObject();
    const QJsonObject orientation = data.value("orientation").toObject();

    m_robotPosition = QPointF(position.value("x").toDouble(), position.value("y").toDouble());
    m_robotTheta = orientation.value("theta").toDouble();
    m_hasOdom = true;
    update();
}

void SlamMapWidget::setPathData(const QJsonObject &data)
{
    const QJsonArray poses = data.value("poses").toArray();
    QVector<QPointF> pathPoints;
    pathPoints.reserve(poses.size());

    for (const QJsonValue &poseValue : poses) {
        const QJsonObject pose = poseValue.toObject();
        pathPoints.append(QPointF(pose.value("x").toDouble(), pose.value("y").toDouble()));
    }

    m_pathPoints = pathPoints;
    update();
}

void SlamMapWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), QColor(15, 23, 42));

    if (!m_hasMap || m_mapImage.isNull()) {
        painter.setPen(QColor(148, 163, 184));
        painter.drawText(rect(), Qt::AlignCenter, "Waiting for SLAM map...");
        return;
    }

    const QRectF mapRect = fittedMapRect();
    painter.drawImage(mapRect, m_mapImage);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(59, 130, 246), 2.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(mapRect);

    const qreal scaleX = mapRect.width() / static_cast<qreal>(m_mapWidth);
    const qreal scaleY = mapRect.height() / static_cast<qreal>(m_mapHeight);
    const auto toWidgetPoint = [&](const QPointF &worldPoint) {
        const QPointF imagePoint = worldToImage(worldPoint);
        return QPointF(mapRect.left() + (imagePoint.x() * scaleX),
                       mapRect.top() + (imagePoint.y() * scaleY));
    };

    if (m_pathPoints.size() > 1) {
        QPainterPath pathOverlay;
        pathOverlay.moveTo(toWidgetPoint(m_pathPoints.first()));
        for (int i = 1; i < m_pathPoints.size(); ++i) {
            pathOverlay.lineTo(toWidgetPoint(m_pathPoints.at(i)));
        }

        painter.setPen(QPen(QColor(34, 211, 238), 3.0));
        painter.drawPath(pathOverlay);
    }

    if (m_hasOdom) {
        const QPointF robotCenter = toWidgetPoint(m_robotPosition);
        const qreal robotRadius = 6.0;
        const qreal arrowLength = 18.0;
        const QPointF heading(std::cos(m_robotTheta) * arrowLength,
                              -std::sin(m_robotTheta) * arrowLength);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(249, 115, 22));
        painter.drawEllipse(robotCenter, robotRadius, robotRadius);

        painter.setPen(QPen(QColor(251, 146, 60), 3.0));
        painter.drawLine(robotCenter, robotCenter + heading);
    }
}

QPointF SlamMapWidget::worldToImage(const QPointF &worldPoint) const
{
    if (m_resolution <= 0.0 || m_mapHeight <= 0) {
        return QPointF();
    }

    const qreal x = (worldPoint.x() - m_origin.x()) / m_resolution;
    const qreal y = m_mapHeight - ((worldPoint.y() - m_origin.y()) / m_resolution);
    return QPointF(x, y);
}

QRectF SlamMapWidget::fittedMapRect() const
{
    const QRectF available = rect().adjusted(8, 8, -8, -8);
    if (available.isEmpty() || m_mapImage.isNull()) {
        return available;
    }

    QSizeF imageSize = m_mapImage.size();
    imageSize.scale(available.size(), Qt::KeepAspectRatio);

    const qreal x = available.left() + ((available.width() - imageSize.width()) * 0.5);
    const qreal y = available.top() + ((available.height() - imageSize.height()) * 0.5);
    return QRectF(QPointF(x, y), imageSize);
}
