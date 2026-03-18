#include "slammapwidget.h"

#include <cmath>
#include <limits>
#include <QDebug>
#include <QJsonArray>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {

constexpr double kMinZoomFactor = 1.0;
constexpr double kMaxZoomFactor = 12.0;
constexpr double kZoomStepFactor = 1.15;
constexpr double kPanActivationEpsilon = 0.001;

QColor interpolateColor(const QColor &from, const QColor &to, qreal ratio)
{
    const qreal clampedRatio = qBound<qreal>(0.0, ratio, 1.0);
    return QColor(
        qRound(from.red() + ((to.red() - from.red()) * clampedRatio)),
        qRound(from.green() + ((to.green() - from.green()) * clampedRatio)),
        qRound(from.blue() + ((to.blue() - from.blue()) * clampedRatio)));
}

QString ownedTrimmedString(const QJsonValue &value)
{
    const QString raw = value.toString();
    QStringView view(raw);

    while (!view.isEmpty() && view.front().isSpace()) {
        view = view.sliced(1);
    }
    while (!view.isEmpty() && view.back().isSpace()) {
        view.chop(1);
    }

    return view.toString();
}

QString extractFrameId(const QJsonObject &data)
{
    const QString directFrameId = ownedTrimmedString(data.value("frame_id"));
    if (!directFrameId.isEmpty()) {
        return directFrameId;
    }

    const QJsonObject header = data.value("header").toObject();
    const QString headerFrameId = ownedTrimmedString(header.value("frame_id"));
    if (!headerFrameId.isEmpty()) {
        return headerFrameId;
    }

    const QJsonObject pose = data.value("pose").toObject();
    if (!pose.isEmpty()) {
        return extractFrameId(pose);
    }

    const QJsonObject transform = data.value("transform").toObject();
    if (!transform.isEmpty()) {
        return extractFrameId(transform);
    }

    const QJsonObject wrapperMsg = data.value("msg").toObject();
    if (!wrapperMsg.isEmpty()) {
        return extractFrameId(wrapperMsg);
    }

    const QJsonObject wrapperData = data.value("data").toObject();
    if (!wrapperData.isEmpty()) {
        return extractFrameId(wrapperData);
    }

    const QJsonObject wrapperPayload = data.value("payload").toObject();
    if (!wrapperPayload.isEmpty()) {
        return extractFrameId(wrapperPayload);
    }

    return {};
}

QPointF extractPoint(const QJsonObject &data)
{
    const QJsonObject position = data.value("position").toObject();
    if (!position.isEmpty()) {
        return QPointF(position.value("x").toDouble(), position.value("y").toDouble());
    }

    const QJsonObject translation = data.value("translation").toObject();
    if (!translation.isEmpty()) {
        return QPointF(translation.value("x").toDouble(), translation.value("y").toDouble());
    }

    const QJsonObject pose = data.value("pose").toObject();
    if (!pose.isEmpty()) {
        return extractPoint(pose);
    }

    const QJsonObject transform = data.value("transform").toObject();
    if (!transform.isEmpty()) {
        return extractPoint(transform);
    }

    const QJsonObject wrapperMsg = data.value("msg").toObject();
    if (!wrapperMsg.isEmpty()) {
        return extractPoint(wrapperMsg);
    }

    const QJsonObject wrapperData = data.value("data").toObject();
    if (!wrapperData.isEmpty()) {
        return extractPoint(wrapperData);
    }

    const QJsonObject wrapperPayload = data.value("payload").toObject();
    if (!wrapperPayload.isEmpty()) {
        return extractPoint(wrapperPayload);
    }

    return QPointF(data.value("x").toDouble(), data.value("y").toDouble());
}

double extractYaw(const QJsonObject &data)
{
    if (data.contains("theta")) {
        return data.value("theta").toDouble();
    }

    if (data.contains("yaw")) {
        return data.value("yaw").toDouble();
    }

    if (data.contains("heading")) {
        return data.value("heading").toDouble();
    }

    if (data.contains("angle")) {
        return data.value("angle").toDouble();
    }

    const QJsonObject orientation = data.value("orientation").toObject();
    if (!orientation.isEmpty()) {
        return extractYaw(orientation);
    }

    const QJsonObject pose = data.value("pose").toObject();
    if (!pose.isEmpty()) {
        return extractYaw(pose);
    }

    const QJsonObject rotation = data.value("rotation").toObject();
    if (!rotation.isEmpty()) {
        return extractYaw(rotation);
    }

    const QJsonObject transform = data.value("transform").toObject();
    if (!transform.isEmpty()) {
        return extractYaw(transform);
    }

    const QJsonObject wrapperMsg = data.value("msg").toObject();
    if (!wrapperMsg.isEmpty()) {
        return extractYaw(wrapperMsg);
    }

    const QJsonObject wrapperData = data.value("data").toObject();
    if (!wrapperData.isEmpty()) {
        return extractYaw(wrapperData);
    }

    const QJsonObject wrapperPayload = data.value("payload").toObject();
    if (!wrapperPayload.isEmpty()) {
        return extractYaw(wrapperPayload);
    }

    if (data.contains("x") || data.contains("y") || data.contains("z") || data.contains("w")) {
        const double x = data.value("x").toDouble();
        const double y = data.value("y").toDouble();
        const double z = data.value("z").toDouble();
        const double w = data.contains("w") ? data.value("w").toDouble() : 1.0;
        const double sinyCosp = 2.0 * ((w * z) + (x * y));
        const double cosyCosp = 1.0 - (2.0 * ((y * y) + (z * z)));
        return std::atan2(sinyCosp, cosyCosp);
    }

    return 0.0;
}

bool isFrameCompatible(const QString &frameId, const QString &mapFrameId)
{
    return frameId.isEmpty() || frameId.compare(mapFrameId, Qt::CaseInsensitive) == 0;
}

bool imagePointWithinBounds(const QPointF &point, int width, int height)
{
    return point.x() >= 0.0 && point.x() <= static_cast<qreal>(width - 1)
        && point.y() >= 0.0 && point.y() <= static_cast<qreal>(height - 1);
}

qreal distanceOutsideBounds(const QPointF &point, int width, int height)
{
    const qreal maxX = static_cast<qreal>(width - 1);
    const qreal maxY = static_cast<qreal>(height - 1);
    const qreal dx = (point.x() < 0.0) ? -point.x() : ((point.x() > maxX) ? (point.x() - maxX) : 0.0);
    const qreal dy = (point.y() < 0.0) ? -point.y() : ((point.y() > maxY) ? (point.y() - maxY) : 0.0);
    return dx + dy;
}

} // namespace

SlamMapWidget::SlamMapWidget(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(0, 0);
}

void SlamMapWidget::setGoalTargetingEnabled(bool enabled)
{
    m_goalTargetingEnabled = enabled;
    setMouseTracking(enabled);
    if (!enabled) {
        m_isSettingGoalDirection = false;
        m_goalStartWidgetPos = QPointF();
        m_goalEndWidgetPos = QPointF();
    }
    updateInteractionCursor();
    update();
}

bool SlamMapWidget::hasMapData() const
{
    return m_hasMap && !m_mapImage.isNull() && m_mapWidth > 0 && m_mapHeight > 0 && m_resolution > 0.0;
}

QPointF SlamMapWidget::mapOrigin() const
{
    return m_origin;
}

double SlamMapWidget::mapResolution() const
{
    return m_resolution;
}

int SlamMapWidget::mapWidthCells() const
{
    return m_mapWidth;
}

int SlamMapWidget::mapHeightCells() const
{
    return m_mapHeight;
}

void SlamMapWidget::clearGoalOverlay()
{
    m_isSettingGoalDirection = false;
    m_goalStartWidgetPos = QPointF();
    m_goalEndWidgetPos = QPointF();
    m_hasCommittedGoalArrow = false;
    m_committedGoalStartWorldPos = QPointF();
    m_committedGoalEndWorldPos = QPointF();
    m_pathPoints.clear();
    update();
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

    const bool hadMap = hasMapData();
    const bool geometryChanged = (width != m_mapWidth) || (height != m_mapHeight);

    const QColor unknownColor(30, 30, 46);
    const QColor freeColor(255, 255, 255);
    const QColor wallColor(0, 0, 0);

    QImage mapImage(width, height, QImage::Format_RGB32);
    for (int y = 0; y < height; ++y) {
        QRgb *scanLine = reinterpret_cast<QRgb *>(mapImage.scanLine(height - 1 - y));
        for (int x = 0; x < width; ++x) {
            const int value = cells.at((y * width) + x).toInt();

            QColor color;
            if (value < 0) {
                color = unknownColor;
            } else {
                const int clamped = qBound(0, value, 100);
                if (clamped <= 15) {
                    color = freeColor;
                } else if (clamped >= 65) {
                    color = wallColor;
                } else {
                    const qreal blendRatio = static_cast<qreal>(clamped - 15) / 50.0;
                    color = interpolateColor(freeColor, wallColor, blendRatio);
                }
            }

            scanLine[x] = color.rgb();
        }
    }

    m_mapImage = mapImage;
    m_mapWidth = width;
    m_mapHeight = height;
    m_resolution = resolution;
    m_origin = extractPoint(origin);
    m_originYaw = extractYaw(origin);
    const QString frameId = extractFrameId(data);
    m_mapFrameId = frameId.isEmpty() ? QString("map") : frameId;
    m_hasMap = true;
    if (!hadMap || geometryChanged || m_viewCenterImage.isNull()) {
        m_projectionMode = ProjectionMode::Standard;
        resetView();
        return;
    }

    clampViewCenter();
    update();
}

void SlamMapWidget::setOdomData(const QJsonObject &data)
{
    const QJsonObject mapPose = data.value("map_pose").toObject();
    const QJsonObject mapPosition = data.value("map_position").toObject();
    const QJsonObject mapOrientation = data.value("map_orientation").toObject();
    const QString frameId = !mapPose.isEmpty() ? extractFrameId(mapPose) : extractFrameId(data);
    const bool hasMapAlignedPose = !mapPose.isEmpty() || !mapPosition.isEmpty() || !mapOrientation.isEmpty();

    if (!hasMapAlignedPose && !isFrameCompatible(frameId, m_mapFrameId)) {
        if (m_lastRejectedOdomFrameId != frameId) {
            qWarning() << "[SlamMapWidget] Received robot pose in frame" << frameId
                       << "for map frame" << m_mapFrameId
                       << "- rendering raw pose as fallback until map-frame pose is available.";
            m_lastRejectedOdomFrameId = frameId;
        }
    } else {
        m_lastRejectedOdomFrameId = QString();
    }

    m_robotPosition = extractPoint(mapPosition.isEmpty() ? (mapPose.isEmpty() ? data : mapPose) : mapPosition);
    m_robotTheta = extractYaw(mapOrientation.isEmpty() ? (mapPose.isEmpty() ? data : mapPose) : mapOrientation);
    m_hasOdom = true;

    const ProjectionMode nextProjectionMode = bestProjectionModeForWorldPoint(m_robotPosition);
    if (m_projectionMode != nextProjectionMode) {
        qDebug() << "[SlamMapWidget] Projection mode ->" << projectionModeLabel(nextProjectionMode);
        m_projectionMode = nextProjectionMode;
    }

    static int odomParsePreviewCount = 0;
    if (odomParsePreviewCount < 3) {
        qDebug() << "[SlamMapWidget] Parsed odom -> pos:" << m_robotPosition
                 << "yaw:" << m_robotTheta
                 << "frame:" << frameId;
        ++odomParsePreviewCount;
    }

    update();
}

void SlamMapWidget::setPathData(const QJsonObject &data)
{
    const QString frameId = extractFrameId(data);
    const bool hasMapAlignedPath = data.contains("map_poses");
    if (!hasMapAlignedPath && !isFrameCompatible(frameId, m_mapFrameId)) {
        if (m_lastRejectedPathFrameId != frameId) {
            qWarning() << "[SlamMapWidget] Received path in frame" << frameId
                       << "for map frame" << m_mapFrameId
                       << "- rendering raw path as fallback until map-frame path is available.";
            m_lastRejectedPathFrameId = frameId;
        }
    } else {
        m_lastRejectedPathFrameId = QString();
    }

    const QJsonArray poses = data.contains("map_poses") ? data.value("map_poses").toArray()
                                                        : data.value("poses").toArray();
    QVector<QPointF> pathPoints;
    pathPoints.reserve(poses.size());

    for (const QJsonValue &poseValue : poses) {
        const QJsonObject pose = poseValue.toObject();
        pathPoints.append(extractPoint(pose));
    }

    m_pathPoints = pathPoints;
    update();
}

void SlamMapWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 30, 46));

    if (!m_hasMap || m_mapImage.isNull()) {
        painter.setPen(QColor(191, 219, 254));
        painter.drawText(rect(), Qt::AlignCenter, "Waiting for SLAM map...");
        return;
    }

    const QRectF mapRect = fittedMapRect();
    const QRectF sourceRect = visibleImageRect();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(mapRect, m_mapImage, sourceRect);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const auto toWidgetPoint = [this](const QPointF &worldPoint) {
        return imageToWidget(worldToImage(worldPoint));
    };

    painter.save();
    painter.setClipRect(mapRect);

    if (m_pathPoints.size() > 1) {
        QPainterPath pathOverlay;
        pathOverlay.moveTo(toWidgetPoint(m_pathPoints.first()));
        for (int i = 1; i < m_pathPoints.size(); ++i) {
            pathOverlay.lineTo(toWidgetPoint(m_pathPoints.at(i)));
        }

        painter.setPen(QPen(QColor(250, 204, 21), 3.0));
        painter.drawPath(pathOverlay);
    }

    if (m_hasOdom) {
        const QPointF robotCenter = toWidgetPoint(m_robotPosition);
        const qreal robotRadius = 6.0;
        const qreal arrowLengthPixels = 18.0;
        const qreal pixelsPerCellX = mapRect.width() / sourceRect.width();
        const qreal pixelsPerCellY = mapRect.height() / sourceRect.height();
        const qreal averagePixelsPerCell = qMax<qreal>(1.0, (pixelsPerCellX + pixelsPerCellY) * 0.5);
        const qreal arrowLengthWorld = (arrowLengthPixels / averagePixelsPerCell) * m_resolution;
        const QPointF robotHeadingWorld(m_robotPosition.x() + (std::cos(m_robotTheta) * arrowLengthWorld),
                                        m_robotPosition.y() + (std::sin(m_robotTheta) * arrowLengthWorld));
        const QPointF headingEnd = toWidgetPoint(robotHeadingWorld);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(249, 115, 22));
        painter.drawEllipse(robotCenter, robotRadius, robotRadius);

        painter.setPen(QPen(QColor(251, 146, 60), 3.0));
        painter.drawLine(robotCenter, headingEnd);
    }

    const auto drawGoalArrow = [&painter](const QPointF &startPos, const QPointF &endPos) {
        if (startPos.isNull()) {
            return;
        }

        painter.setPen(QPen(Qt::red, 3.0));
        painter.setBrush(Qt::red);
        painter.drawEllipse(startPos, 5.0, 5.0);

        if (endPos.isNull() || endPos == startPos) {
            return;
        }

        painter.drawLine(startPos, endPos);

        const double angle = std::atan2(endPos.y() - startPos.y(), endPos.x() - startPos.x());
        const double arrowSize = 12.0;
        const QPointF p1 = endPos - QPointF(arrowSize * std::cos(angle - (M_PI / 6.0)),
                                            arrowSize * std::sin(angle - (M_PI / 6.0)));
        const QPointF p2 = endPos - QPointF(arrowSize * std::cos(angle + (M_PI / 6.0)),
                                            arrowSize * std::sin(angle + (M_PI / 6.0)));
        QPolygonF arrowHead;
        arrowHead << endPos << p1 << p2;
        painter.drawPolygon(arrowHead);
    };

    if (m_hasCommittedGoalArrow) {
        drawGoalArrow(toWidgetPoint(m_committedGoalStartWorldPos),
                      toWidgetPoint(m_committedGoalEndWorldPos));
    }

    if (m_goalTargetingEnabled && !m_goalStartWidgetPos.isNull()) {
        drawGoalArrow(m_goalStartWidgetPos, m_goalEndWidgetPos);
    }

    painter.restore();

    painter.setPen(QPen(QColor(125, 211, 252, 170), 2.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(mapRect);
}

QPointF SlamMapWidget::worldToImage(const QPointF &worldPoint) const
{
    return worldToImage(worldPoint, m_projectionMode);
}

QPointF SlamMapWidget::worldToImage(const QPointF &worldPoint, ProjectionMode mode) const
{
    if (m_resolution <= 0.0 || m_mapWidth <= 0 || m_mapHeight <= 0) {
        return QPointF();
    }

    if (mode == ProjectionMode::Raw) {
        return QPointF(worldPoint.x() / m_resolution,
                       (m_mapHeight - 1) - (worldPoint.y() / m_resolution));
    }

    const QPointF translated = worldPoint - m_origin;
    if (mode == ProjectionMode::NoRotation) {
        return QPointF(translated.x() / m_resolution,
                       (m_mapHeight - 1) - (translated.y() / m_resolution));
    }

    const qreal cosYaw = std::cos(m_originYaw);
    const qreal sinYaw = std::sin(m_originYaw);
    const qreal localX = (translated.x() * cosYaw) + (translated.y() * sinYaw);
    const qreal localY = (-translated.x() * sinYaw) + (translated.y() * cosYaw);
    return QPointF(localX / m_resolution,
                   (m_mapHeight - 1) - (localY / m_resolution));
}

QPointF SlamMapWidget::imageToWorld(const QPointF &imagePoint, ProjectionMode mode) const
{
    if (m_resolution <= 0.0 || m_mapWidth <= 0 || m_mapHeight <= 0) {
        return QPointF();
    }

    const qreal localX = imagePoint.x() * m_resolution;
    const qreal localY = ((m_mapHeight - 1) - imagePoint.y()) * m_resolution;

    if (mode == ProjectionMode::Raw) {
        return QPointF(localX, localY);
    }

    if (mode == ProjectionMode::NoRotation) {
        return QPointF(m_origin.x() + localX, m_origin.y() + localY);
    }

    const qreal cosYaw = std::cos(m_originYaw);
    const qreal sinYaw = std::sin(m_originYaw);
    return QPointF(m_origin.x() + (localX * cosYaw) - (localY * sinYaw),
                   m_origin.y() + (localX * sinYaw) + (localY * cosYaw));
}

void SlamMapWidget::wheelEvent(QWheelEvent *event)
{
    if (!hasMapData() || m_isSettingGoalDirection || event->angleDelta().y() == 0) {
        QWidget::wheelEvent(event);
        return;
    }

    const double steps = static_cast<double>(event->angleDelta().y()) / 120.0;
    const double nextZoom = m_zoomFactor * std::pow(kZoomStepFactor, steps);
    setZoomFactor(nextZoom, event->position());
    event->accept();
}

void SlamMapWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!m_goalTargetingEnabled && event->button() == Qt::LeftButton && hasMapData()) {
        resetView();
        event->accept();
        return;
    }

    QWidget::mouseDoubleClickEvent(event);
}

QPointF SlamMapWidget::widgetToImage(const QPointF &widgetPoint, bool *ok, bool requireInsideMapRect) const
{
    if (ok) {
        *ok = false;
    }

    if (!hasMapData()) {
        return QPointF();
    }

    const QRectF mapRect = fittedMapRect();
    if (mapRect.isEmpty() || (requireInsideMapRect && !mapRect.contains(widgetPoint))) {
        return QPointF();
    }

    const QRectF sourceRect = visibleImageRect();
    if (sourceRect.isEmpty()) {
        return QPointF();
    }

    const qreal normalizedX = (widgetPoint.x() - mapRect.left()) / mapRect.width();
    const qreal normalizedY = (widgetPoint.y() - mapRect.top()) / mapRect.height();
    const qreal imageX = sourceRect.left() + (normalizedX * sourceRect.width());
    const qreal imageY = sourceRect.top() + (normalizedY * sourceRect.height());

    if (ok) {
        *ok = true;
    }
    return QPointF(imageX, imageY);
}

QPointF SlamMapWidget::imageToWidget(const QPointF &imagePoint) const
{
    if (!hasMapData()) {
        return QPointF();
    }

    const QRectF mapRect = fittedMapRect();
    const QRectF sourceRect = visibleImageRect();
    if (mapRect.isEmpty() || sourceRect.isEmpty()) {
        return QPointF();
    }

    const qreal scaleX = mapRect.width() / sourceRect.width();
    const qreal scaleY = mapRect.height() / sourceRect.height();
    return QPointF(mapRect.left() + ((imagePoint.x() - sourceRect.left()) * scaleX),
                   mapRect.top() + ((imagePoint.y() - sourceRect.top()) * scaleY));
}

QPointF SlamMapWidget::widgetToWorld(const QPointF &widgetPoint, bool *ok, bool requireInsideMapRect) const
{
    if (ok) {
        *ok = false;
    }

    const QPointF imagePoint = widgetToImage(widgetPoint, ok, requireInsideMapRect);
    if (ok && !*ok) {
        return QPointF();
    }

    if (!ok && !hasMapData()) {
        return QPointF();
    }

    if (ok) {
        *ok = true;
    }
    return imageToWorld(imagePoint, m_projectionMode);
}

void SlamMapWidget::mousePressEvent(QMouseEvent *event)
{
    if (!m_goalTargetingEnabled && event->button() == Qt::LeftButton && canPan()) {
        bool ok = false;
        widgetToImage(event->position(), &ok);
        if (ok) {
            m_isPanning = true;
            m_lastPanWidgetPos = event->position();
            updateInteractionCursor();
            event->accept();
            return;
        }
    }

    if (!m_goalTargetingEnabled || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    bool ok = false;
    const QPointF goalWorldPoint = widgetToWorld(event->position(), &ok);
    if (!ok) {
        QWidget::mousePressEvent(event);
        return;
    }

    emit goalInteractionStarted();
    Q_UNUSED(goalWorldPoint);
    m_hasCommittedGoalArrow = false;
    m_goalStartWidgetPos = event->position();
    m_goalEndWidgetPos = m_goalStartWidgetPos;
    m_isSettingGoalDirection = true;
    update();
    event->accept();
}

void SlamMapWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isPanning) {
        const QRectF mapRect = fittedMapRect();
        const QRectF sourceRect = visibleImageRect();
        if (!mapRect.isEmpty() && !sourceRect.isEmpty()) {
            const QPointF delta = event->position() - m_lastPanWidgetPos;
            m_viewCenterImage -= QPointF(delta.x() * (sourceRect.width() / mapRect.width()),
                                         delta.y() * (sourceRect.height() / mapRect.height()));
            clampViewCenter();
            m_lastPanWidgetPos = event->position();
            update();
        }
        event->accept();
        return;
    }

    if (m_goalTargetingEnabled && m_isSettingGoalDirection) {
        m_goalEndWidgetPos = event->position();
        update();
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void SlamMapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_isPanning && event->button() == Qt::LeftButton) {
        m_isPanning = false;
        updateInteractionCursor();
        event->accept();
        return;
    }

    if (!m_goalTargetingEnabled || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    if (!m_isSettingGoalDirection) {
        event->accept();
        return;
    }

    m_goalEndWidgetPos = event->position();

    bool ok = false;
    const QPointF goalWorldPoint = widgetToWorld(m_goalStartWidgetPos, &ok);
    if (!ok) {
        m_isSettingGoalDirection = false;
        m_goalStartWidgetPos = QPointF();
        m_goalEndWidgetPos = QPointF();
        update();
        event->accept();
        return;
    }

    const QPointF goalEndWorldPoint = widgetToWorld(m_goalEndWidgetPos, nullptr, false);
    const QPointF widgetDelta = m_goalEndWidgetPos - m_goalStartWidgetPos;
    const qreal dragDistance = std::hypot(widgetDelta.x(), widgetDelta.y());

    double yaw = m_hasOdom ? m_robotTheta : 0.0;
    QPointF committedEndWorldPoint = goalWorldPoint;
    if (dragDistance >= 4.0) {
        yaw = std::atan2(goalEndWorldPoint.y() - goalWorldPoint.y(),
                         goalEndWorldPoint.x() - goalWorldPoint.x());
        committedEndWorldPoint = goalEndWorldPoint;
    }

    m_committedGoalStartWorldPos = goalWorldPoint;
    m_committedGoalEndWorldPos = committedEndWorldPoint;
    m_hasCommittedGoalArrow = true;
    emit goalCommitted();

    m_isSettingGoalDirection = false;
    m_goalStartWidgetPos = QPointF();
    m_goalEndWidgetPos = QPointF();
    update();

    emit goalRequested(goalWorldPoint.x(), goalWorldPoint.y(), yaw);
    event->accept();
}

void SlamMapWidget::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
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

QRectF SlamMapWidget::visibleImageRect() const
{
    if (!hasMapData()) {
        return QRectF();
    }

    const qreal width = static_cast<qreal>(m_mapWidth) / qMax(kMinZoomFactor, m_zoomFactor);
    const qreal height = static_cast<qreal>(m_mapHeight) / qMax(kMinZoomFactor, m_zoomFactor);
    const qreal centerX = m_viewCenterImage.isNull() ? (static_cast<qreal>(m_mapWidth) * 0.5)
                                                     : m_viewCenterImage.x();
    const qreal centerY = m_viewCenterImage.isNull() ? (static_cast<qreal>(m_mapHeight) * 0.5)
                                                     : m_viewCenterImage.y();
    return QRectF(centerX - (width * 0.5), centerY - (height * 0.5), width, height);
}

void SlamMapWidget::resetView()
{
    m_zoomFactor = kMinZoomFactor;
    m_viewCenterImage = QPointF(static_cast<qreal>(m_mapWidth) * 0.5,
                                static_cast<qreal>(m_mapHeight) * 0.5);
    m_isPanning = false;
    updateInteractionCursor();
    update();
}

void SlamMapWidget::setZoomFactor(double zoomFactor, const QPointF &anchorWidgetPos)
{
    if (!hasMapData()) {
        return;
    }

    const double clampedZoom = qBound(kMinZoomFactor, zoomFactor, kMaxZoomFactor);
    if (qFuzzyCompare(m_zoomFactor, clampedZoom)) {
        return;
    }

    const QRectF mapRect = fittedMapRect();
    const bool hasAnchor = mapRect.contains(anchorWidgetPos);
    bool ok = false;
    const QPointF anchorImage = hasAnchor ? widgetToImage(anchorWidgetPos, &ok, false) : QPointF();

    m_zoomFactor = clampedZoom;

    if (hasAnchor && ok) {
        const QRectF nextSourceRect = visibleImageRect();
        const qreal normalizedX = (anchorWidgetPos.x() - mapRect.left()) / mapRect.width();
        const qreal normalizedY = (anchorWidgetPos.y() - mapRect.top()) / mapRect.height();
        m_viewCenterImage = QPointF(anchorImage.x() + ((0.5 - normalizedX) * nextSourceRect.width()),
                                    anchorImage.y() + ((0.5 - normalizedY) * nextSourceRect.height()));
    }

    clampViewCenter();
    updateInteractionCursor();
    update();
}

void SlamMapWidget::clampViewCenter()
{
    if (!hasMapData()) {
        return;
    }

    const qreal width = static_cast<qreal>(m_mapWidth) / qMax(kMinZoomFactor, m_zoomFactor);
    const qreal height = static_cast<qreal>(m_mapHeight) / qMax(kMinZoomFactor, m_zoomFactor);
    const qreal halfWidth = width * 0.5;
    const qreal halfHeight = height * 0.5;

    if (m_viewCenterImage.isNull()) {
        m_viewCenterImage = QPointF(static_cast<qreal>(m_mapWidth) * 0.5,
                                    static_cast<qreal>(m_mapHeight) * 0.5);
    }

    m_viewCenterImage.setX(qBound(halfWidth, m_viewCenterImage.x(),
                                  static_cast<qreal>(m_mapWidth) - halfWidth));
    m_viewCenterImage.setY(qBound(halfHeight, m_viewCenterImage.y(),
                                  static_cast<qreal>(m_mapHeight) - halfHeight));
}

void SlamMapWidget::updateInteractionCursor()
{
    if (m_goalTargetingEnabled) {
        setCursor(Qt::CrossCursor);
        return;
    }

    if (m_isPanning) {
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (canPan()) {
        setCursor(Qt::OpenHandCursor);
        return;
    }

    unsetCursor();
}

bool SlamMapWidget::canPan() const
{
    return hasMapData() && (m_zoomFactor > (kMinZoomFactor + kPanActivationEpsilon));
}

SlamMapWidget::ProjectionMode SlamMapWidget::bestProjectionModeForWorldPoint(const QPointF &worldPoint) const
{
    if (!hasMapData()) {
        return ProjectionMode::Standard;
    }

    const ProjectionMode modes[] = {
        ProjectionMode::Standard,
        ProjectionMode::NoRotation,
        ProjectionMode::Raw
    };

    ProjectionMode bestMode = ProjectionMode::Standard;
    qreal bestDistance = std::numeric_limits<qreal>::max();

    for (const ProjectionMode mode : modes) {
        const QPointF imagePoint = worldToImage(worldPoint, mode);
        const qreal distance = distanceOutsideBounds(imagePoint, m_mapWidth, m_mapHeight);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestMode = mode;
        }
    }

    return bestMode;
}

QString SlamMapWidget::projectionModeLabel(ProjectionMode mode) const
{
    switch (mode) {
    case ProjectionMode::Standard:
        return QStringLiteral("standard");
    case ProjectionMode::NoRotation:
        return QStringLiteral("origin-only");
    case ProjectionMode::Raw:
        return QStringLiteral("raw");
    }

    return QStringLiteral("unknown");
}
