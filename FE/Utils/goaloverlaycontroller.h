#ifndef GOALOVERLAYCONTROLLER_H
#define GOALOVERLAYCONTROLLER_H

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QWidget>

#include <optional>

class VideoWidget;

namespace GoalOverlay {

constexpr double kSharedGoalAspectRatio = 16.0 / 9.0;
constexpr qreal kPointCacheEpsilon = 0.0005;
constexpr qreal kDisplayRectCacheEpsilon = 0.5;
constexpr qreal kCropRectCacheEpsilon = 0.0005;

struct ArrowStyle
{
    QColor color = Qt::red;
    qreal penWidth = 3.0;
    qreal pointRadius = 5.0;
    qreal arrowSize = 12.0;
};

struct CommitResult
{
    QPointF normalizedStart;
    QPointF normalizedEnd;
    QPointF localStart;
    QPointF localEnd;
    QRectF displayRect;
    QRectF cropRect;
    double angleRadians = 0.0;
    bool hasDirection = false;
};

struct ProjectionResult
{
    QPointF localStart;
    QPointF localEnd;
    QRectF displayRect;
    QRectF cropRect;
};

bool nearlyEqual(qreal a, qreal b, qreal epsilon = 0.01);
bool samePoint(const QPointF &lhs, const QPointF &rhs, qreal epsilon = 0.01);
bool sameRect(const QRectF &lhs, const QRectF &rhs, qreal epsilon = 0.01);
double aspectFromDisplayRect(const QRectF &displayRect);
QPointF clampPointToRect(const QPointF &point, const QRectF &rect);
QPointF remapNormalizedBetweenAspects(const QPointF &point, double sourceAspect, double targetAspect);

std::optional<CommitResult> buildCommitResult(VideoWidget *videoWidget,
                                              const QPointF &startPos,
                                              const QPointF &rawEndPos,
                                              double targetAspectRatio = kSharedGoalAspectRatio,
                                              qreal dragThreshold = 4.0);

std::optional<ProjectionResult> projectCommittedGoal(VideoWidget *videoWidget,
                                                     const QPointF &normalizedStart,
                                                     const QPointF &normalizedEnd,
                                                     double sourceAspectRatio = kSharedGoalAspectRatio);

} // namespace GoalOverlay

class GoalArrowOverlayWidget : public QWidget
{
public:
    explicit GoalArrowOverlayWidget(const GoalOverlay::ArrowStyle &style = {}, QWidget *parent = nullptr);

    void syncToWidget(QWidget *target);
    void setClipRect(const QRectF &clipRect);
    void setActive(bool active);
    bool isActive() const;
    bool hasCommittedArrow() const;
    void setPreviewArrow(const QPointF &startPos, const QPointF &endPos);
    void clearPreview();
    void clearAll();
    void setCommittedArrow(const QPointF &startPos, const QPointF &endPos, bool hasArrow);
    void commitArrow(const QPointF &startPos, const QPointF &endPos);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void updateVisibility();

    GoalOverlay::ArrowStyle m_style;
    bool m_active = false;
    bool m_isDrawingArrow = false;
    QPointF m_startPos;
    QPointF m_endPos;
    bool m_hasCommittedArrow = false;
    QPointF m_committedStartPos;
    QPointF m_committedEndPos;
    QRectF m_clipRect;
};

#endif // GOALOVERLAYCONTROLLER_H
