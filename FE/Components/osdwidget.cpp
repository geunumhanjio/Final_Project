#include "osdwidget.h"
#include <QPainter>
#include <QRandomGenerator>

OsdWidget::OsdWidget(QWidget *parent) : QWidget(parent)
{
    // [수정] GStreamer 오버레이보다 무조건 위로 올라오도록 탑레벨 윈도우 속성 부여
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    
    // 입력 이벤트 무시 (클릭 등은 밑에 있는 위젯으로 패스)
    setAttribute(Qt::WA_TransparentForMouseEvents);
    // 배경 투명
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground, true);

    // 기본적으로 아무것도 안 보이게 설정이고 데이터는 Unknown으로 초기화
    for (int i = 0; i < MetricCount; ++i) {
        m_visibility[static_cast<Metric>(i)] = false;
        m_values[static_cast<Metric>(i)] = "Unknown";
    }
}

void OsdWidget::setMetricVisible(Metric metric, bool visible)
{
    m_visibility[metric] = visible;
    update(); // 즉시 다시 그리기
}

bool OsdWidget::isMetricVisible(Metric metric) const
{
    return m_visibility.value(metric, false);
}

void OsdWidget::setMetricValue(Metric metric, const QString &value)
{
    m_values[metric] = value;
    update();
}

QString OsdWidget::getMetricName(Metric metric)
{
    switch (metric) {
        case Protocol:   return "Protocol"; // [New]
        case PacketLoss: return "Packet Loss";
        case Jitter:     return "Jitter";
        case Bitrate:    return "Bitrate";
        case FPS:        return "FPS";
        case Latency:    return "Latency";
        default:         return "Unknown";
    }
}

void OsdWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 표시할 텍스트 목록 수집
    QStringList lines;
    for (int i = 0; i < MetricCount; ++i) {
        Metric m = static_cast<Metric>(i);
        if (m_visibility[m]) {
            lines << QString("%1: %2").arg(getMetricName(m), m_values[m]);
        }
    }

    if (lines.isEmpty()) {
        return; // 그릴 게 없으면 종료
    }

    QFont font("Consolas", 10, QFont::Bold);
    painter.setFont(font);
    QFontMetrics fm(font);

    // 가장 긴 줄 찾기
    int maxWidth = 0;
    for (const QString &line : lines) {
        maxWidth = qMax(maxWidth, fm.horizontalAdvance(line));
    }

    // 텍스트 박스 높이 계산
    int lineHeight = fm.height();
    int padding = 6;
    int totalHeight = (lineHeight * lines.size()) + (lines.size() - 1) * 2 + padding * 2;
    int totalWidth = maxWidth + padding * 2;

    // 우측 상단 배치 (약간의 마진 추가, 비디오카드의 TopBar(높이 40) 아래로 내림)
    int marginX = 10;
    int marginY = 45; // Top bar height is 40, plus 5px padding
    QRect bgRect(width() - totalWidth - marginX, marginY, totalWidth, totalHeight);

    // 반투명 배경 (회색, 알파 150)
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 150));
    painter.drawRoundedRect(bgRect, 4, 4);

    // 텍스트 그리기 (초록색 텍스트 계열)
    painter.setPen(QColor(0, 255, 128));
    int y = bgRect.top() + padding + fm.ascent();
    int x = bgRect.left() + padding;

    for (const QString &line : lines) {
        painter.drawText(x, y, line);
        y += lineHeight + 2;
    }
}
