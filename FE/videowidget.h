#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <QMutex>
#include <QRectF>

class VideoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget();

    // latency: 0이면 전체화면 모드 (버퍼링 끔)
    void playUrl(const QString &url, int latency = 200);
    void stop();
    Q_INVOKABLE void startRetryTimer();
    bool isPlaying() const { return m_isPlaying; }

    // [기능 1] 자르기 (0.0 ~ 1.0 상대 좌표)
    void applyCrop(const QRectF &rect);
    void resetCrop();

    // [기능 2] 화면 이동 (픽셀 단위 이동량 입력)
    void panView(qreal dx, qreal dy);

    // [기능 3] 현재 보고 있는 영역 정보 반환 (히스토리 저장용)
    QRectF getCurrentCrop() const { return currentCropRect; }

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    QPaintEngine *paintEngine() const override { return nullptr; }

private slots:
    void pollGstBus();

private:
    GstElement *pipeline;
    GstElement *cropper; // GStreamer crop 요소
    int sourceWidth, sourceHeight;
    QMutex cropMutex;
    QRectF currentCropRect; // 현재 보고 있는 영역 기억

    QString currentUrl;
    int currentLatency;
    QLabel *placeholderLabel;

    QTimer *retryTimer;
    QTimer *watchdogTimer;
    QTimer *busTimer;

    bool m_isPlaying;

    static GstBusSyncReply busSyncHandler(GstBus *bus, GstMessage *msg, gpointer user_data);
};

#endif // VIDEOWIDGET_H
