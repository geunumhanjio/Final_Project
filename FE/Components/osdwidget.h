#ifndef OSDWIDGET_H
#define OSDWIDGET_H

#include <QWidget>
#include <QMap>
#include <QString>
#include <QTimer>

class OsdWidget : public QWidget
{
    Q_OBJECT
public:
    explicit OsdWidget(QWidget *parent = nullptr);

    enum Metric {
        PacketLoss,
        Jitter,
        Bitrate,
        FPS,
        Latency,
        MetricCount
    };

    // 설정: 특정 지표를 OSD에 표시할지 여부 결정
    void setMetricVisible(Metric metric, bool visible);
    bool isMetricVisible(Metric metric) const;
    
    // 현재 각 지표의 값 설정 (실제 데이터 연동 전까지는 모의 데이터 사용)
    void setMetricValue(Metric metric, const QString &value);

    // 메뉴를 편하게 다루기 위해 Metric 문자열 이름 반환
    static QString getMetricName(Metric metric);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QMap<Metric, bool> m_visibility;
    QMap<Metric, QString> m_values;
};

#endif // OSDWIDGET_H
