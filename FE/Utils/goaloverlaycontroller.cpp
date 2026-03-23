#include "goaloverlaycontroller.h"

#include "videowidget.h"

#include <QPainter>

#include <cmath>

namespace GoalOverlay {

bool nearlyEqual(qreal a, qreal b, qreal epsilon)
{
    return std::abs(a - b) <= epsilon;
}

bool samePoint(const QPointF &lhs, const QPointF &rhs, qreal epsilon)
{
    return nearlyEqual(lhs.x(), rhs.x(), epsilon) && nearlyEqual(lhs.y(), rhs.y(), epsilon);
}

bool sameRect(const QRectF &lhs, const QRectF &rhs, qreal epsilon)
{
    return samePoint(lhs.topLeft(), rhs.topLeft(), epsilon)
           && nearlyEqual(lhs.width(), rhs.width(), epsilon)
           && nearlyEqual(lhs.height(), rhs.height(), epsilon);
}

double aspectFromDisplayRect(const QRectF &displayRect)
{
    if (displayRect.height() <= 0.0) {
        return kSharedGoalAspectRatio;
    }
    return displayRect.width() / displayRect.height();
}

QPointF clampPointToRect(const QPointF &point, const QRectF &rect)
{
    if (!rect.isValid()) {
        return point;
    }

    return QPointF(qBound(rect.left(), point.x(), rect.right()),
                   qBound(rect.top(), point.y(), rect.bottom()));
}

QPointF remapNormalizedBetweenAspects(const QPointF &point, double sourceAspect, double targetAspect)
{
    sourceAspect = sourceAspect > 0.0 ? sourceAspect : kSharedGoalAspectRatio;
    targetAspect = targetAspect > 0.0 ? targetAspect : kSharedGoalAspectRatio;
    if (nearlyEqual(sourceAspect, targetAspect, 0.0001)) {
        return QPointF(qBound(0.0, point.x(), 1.0), qBound(0.0, point.y(), 1.0));
    }

    const double referenceAspect = std::max(sourceAspect, targetAspect);
    const auto toReference = [referenceAspect](const QPointF &srcPoint, double aspect) {
        if (nearlyEqual(aspect, referenceAspect, 0.0001)) {
            return srcPoint;
        }

        const double visibleWidth = qBound(0.0, aspect / referenceAspect, 1.0);
        const double offsetX = (1.0 - visibleWidth) * 0.5;
        return QPointF(offsetX + (srcPoint.x() * visibleWidth), srcPoint.y());
    };

    const auto fromReference = [referenceAspect](const QPointF &refPoint, double aspect) {
        if (nearlyEqual(aspect, referenceAspect, 0.0001)) {
            return refPoint;
        }

        const double visibleWidth = qBound(0.0001, aspect / referenceAspect, 1.0);
        const double offsetX = (1.0 - visibleWidth) * 0.5;
        return QPointF((refPoint.x() - offsetX) / visibleWidth, refPoint.y());
    };

    const QPointF referencePoint = toReference(point, sourceAspect);
    const QPointF mappedPoint = fromReference(referencePoint, targetAspect);
    return QPointF(qBound(0.0, mappedPoint.x(), 1.0),
                   qBound(0.0, mappedPoint.y(), 1.0));
}

std::optional<CommitResult> buildCommitResult(VideoWidget *videoWidget,
                                              const QPointF &startPos,
                                              const QPointF &rawEndPos,
                                              double targetAspectRatio,
                                              qreal dragThreshold)
{
    if (!videoWidget || videoWidget->width() <= 0 || videoWidget->height() <= 0) {
        return std::nullopt;
    }

    const QRectF displayRect = videoWidget->getVideoDisplayRect();
    if (displayRect.isEmpty()) {
        return std::nullopt;
    }

    const QPointF clampedEnd = clampPointToRect(rawEndPos, displayRect);
    bool ok = false;
    const QPointF localNormalizedStart = videoWidget->widgetPointToVideoNormalized(startPos, &ok);
    if (!ok) {
        return std::nullopt;
    }

    const QPointF localNormalizedEnd = videoWidget->widgetPointToVideoNormalized(clampedEnd, &ok);
    if (!ok) {
        return std::nullopt;
    }

    const double localAspectRatio = aspectFromDisplayRect(displayRect);
    CommitResult result;
    result.normalizedStart = remapNormalizedBetweenAspects(localNormalizedStart,
                                                           localAspectRatio,
                                                           targetAspectRatio);
    result.normalizedEnd = result.normalizedStart;
    result.localStart = startPos;
    result.localEnd = startPos;
    result.displayRect = displayRect;
    result.cropRect = videoWidget->getCurrentCrop();

    const QPointF normalizedEnd = remapNormalizedBetweenAspects(localNormalizedEnd,
                                                                localAspectRatio,
                                                                targetAspectRatio);
    const QPointF delta = clampedEnd - startPos;
    const qreal dragDistance = std::hypot(delta.x(), delta.y());
    if (dragDistance >= dragThreshold) {
        result.normalizedEnd = normalizedEnd;
        result.localEnd = clampedEnd;
        result.angleRadians = std::atan2(startPos.y() - clampedEnd.y(),
                                         clampedEnd.x() - startPos.x());
        result.hasDirection = true;
    }

    return result;
}

std::optional<ProjectionResult> projectCommittedGoal(VideoWidget *videoWidget,
                                                     const QPointF &normalizedStart,
                                                     const QPointF &normalizedEnd,
                                                     double sourceAspectRatio)
{
    if (!videoWidget || videoWidget->width() <= 0 || videoWidget->height() <= 0) {
        return std::nullopt;
    }

    const QRectF displayRect = videoWidget->getVideoDisplayRect();
    if (displayRect.isEmpty()) {
        return std::nullopt;
    }

    const double targetAspectRatio = aspectFromDisplayRect(displayRect);
    const QPointF localStartNormalized = remapNormalizedBetweenAspects(normalizedStart,
                                                                       sourceAspectRatio,
                                                                       targetAspectRatio);
    const QPointF localEndNormalized = remapNormalizedBetweenAspects(normalizedEnd,
                                                                     sourceAspectRatio,
                                                                     targetAspectRatio);

    ProjectionResult result;
    result.localStart = videoWidget->videoNormalizedToWidgetPoint(localStartNormalized);
    result.localEnd = videoWidget->videoNormalizedToWidgetPoint(localEndNormalized);
    result.displayRect = displayRect;
    result.cropRect = videoWidget->getCurrentCrop();
    return result;
}

} // namespace GoalOverlay

GoalArrowOverlayWidget::GoalArrowOverlayWidget(const GoalOverlay::ArrowStyle &style, QWidget *parent)
    : QWidget(parent)
    , m_style(style)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_StyledBackground, false);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAutoFillBackground(false);
    hide();
}

void GoalArrowOverlayWidget::syncToWidget(QWidget *target)
{
    if (!target) {
        return;
    }

    QWidget *ownerWindow = target->window();
    if (ownerWindow && parentWidget() != ownerWindow) {
        setParent(ownerWindow, windowFlags());
    }

    const QRect targetGeometry(target->mapToGlobal(QPoint(0, 0)), target->size());
    if (geometry() != targetGeometry) {
        setGeometry(targetGeometry);
    }
}

void GoalArrowOverlayWidget::setClipRect(const QRectF &clipRect)
{
    m_clipRect = clipRect;
    update();
}

void GoalArrowOverlayWidget::setActive(bool active)
{
    m_active = active;
    if (!m_active) {
        m_isDrawingArrow = false;
        m_startPos = QPointF();
        m_endPos = QPointF();
    }
    updateVisibility();
    update();
}

bool GoalArrowOverlayWidget::isActive() const
{
    return m_active;
}

bool GoalArrowOverlayWidget::hasCommittedArrow() const
{
    return m_hasCommittedArrow;
}

void GoalArrowOverlayWidget::setPreviewArrow(const QPointF &startPos, const QPointF &endPos)
{
    m_startPos = startPos;
    m_endPos = endPos;
    m_isDrawingArrow = true;
    updateVisibility();
    update();
}

void GoalArrowOverlayWidget::clearPreview()
{
    m_startPos = QPointF();
    m_endPos = QPointF();
    m_isDrawingArrow = false;
    updateVisibility();
    update();
}

void GoalArrowOverlayWidget::clearAll()
{
    clearPreview();
    setCommittedArrow(QPointF(), QPointF(), false);
}

void GoalArrowOverlayWidget::setCommittedArrow(const QPointF &startPos, const QPointF &endPos, bool hasArrow)
{
    const bool changed = (m_hasCommittedArrow != hasArrow)
                         || (m_committedStartPos != startPos)
                         || (m_committedEndPos != endPos);
    m_hasCommittedArrow = hasArrow;
    m_committedStartPos = hasArrow ? startPos : QPointF();
    m_committedEndPos = hasArrow ? endPos : QPointF();
    updateVisibility();
    if (changed) {
        update();
    }
}

void GoalArrowOverlayWidget::commitArrow(const QPointF &startPos, const QPointF &endPos)
{
    clearPreview();
    setCommittedArrow(startPos, endPos, true);
}

void GoalArrowOverlayWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setClipRect(m_clipRect.isValid() ? m_clipRect : QRectF(rect()));

    const auto drawArrow = [this, &painter](const QPointF &startPos, const QPointF &endPos) {
        if (startPos.isNull()) {
            return;
        }

        QPen pen(m_style.color, m_style.penWidth);
        painter.setPen(pen);
        painter.setBrush(m_style.color);
        painter.drawEllipse(startPos, m_style.pointRadius, m_style.pointRadius);

        if (endPos.isNull() || endPos == startPos) {
            return;
        }

        painter.drawLine(startPos, endPos);

        const double angle = std::atan2(endPos.y() - startPos.y(), endPos.x() - startPos.x());
        const QPointF p1 = endPos - QPointF(m_style.arrowSize * std::cos(angle - (M_PI / 6.0)),
                                            m_style.arrowSize * std::sin(angle - (M_PI / 6.0)));
        const QPointF p2 = endPos - QPointF(m_style.arrowSize * std::cos(angle + (M_PI / 6.0)),
                                            m_style.arrowSize * std::sin(angle + (M_PI / 6.0)));
        QPolygonF arrowHead;
        arrowHead << endPos << p1 << p2;
        painter.drawPolygon(arrowHead);
    };

    if (m_hasCommittedArrow) {
        drawArrow(m_committedStartPos, m_committedEndPos);
    }
    if (m_active && m_isDrawingArrow && !m_startPos.isNull()) {
        drawArrow(m_startPos, m_endPos);
    }
}

void GoalArrowOverlayWidget::updateVisibility()
{
    if (m_hasCommittedArrow || (m_active && m_isDrawingArrow && !m_startPos.isNull())) {
        if (!isVisible()) {
            show();
        }
    } else if (isVisible()) {
        hide();
    }
}
