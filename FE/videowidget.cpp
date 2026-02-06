#include "videowidget.h"
#include <QVBoxLayout>
#include <QDebug>
#include <QApplication>
#include <QMutexLocker>

VideoWidget::VideoWidget(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_StyledBackground, true);
    this->setStyleSheet("background-color: black;");
    setMinimumSize(160, 120);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    placeholderLabel = new QLabel("No Signal", this);
    placeholderLabel->setAlignment(Qt::AlignCenter);
    placeholderLabel->setStyleSheet("color: white; font-size: 16px; font-weight: bold;");
    layout->addWidget(placeholderLabel);

    pipeline = nullptr;
    cropper = nullptr;
    m_isPlaying = false;
    currentLatency = 200;
    sourceWidth = 1920; // FHD 가정
    sourceHeight = 1080;
    currentCropRect = QRectF(0.0, 0.0, 1.0, 1.0); // 초기값: 전체 화면

    retryTimer = new QTimer(this);
    retryTimer->setSingleShot(true);
    connect(retryTimer, &QTimer::timeout, [this](){
        if (!currentUrl.isEmpty()) playUrl(currentUrl, currentLatency);
    });

    watchdogTimer = new QTimer(this);
    watchdogTimer->setSingleShot(true);
    connect(watchdogTimer, &QTimer::timeout, [this](){
        if(m_isPlaying) return;
        stop();
        startRetryTimer();
    });

    busTimer = new QTimer(this);
    connect(busTimer, &QTimer::timeout, this, &VideoWidget::pollGstBus);
}

VideoWidget::~VideoWidget() { stop(); }

// [확대] 특정 영역 자르기
void VideoWidget::applyCrop(const QRectF &rect)
{
    QMutexLocker locker(&cropMutex);
    currentCropRect = rect; // 현재 상태 저장

    if (!cropper || !m_isPlaying) return;

    int left = static_cast<int>(rect.left() * sourceWidth);
    int right = static_cast<int>((1.0 - rect.right()) * sourceWidth);
    int top = static_cast<int>(rect.top() * sourceHeight);
    int bottom = static_cast<int>((1.0 - rect.bottom()) * sourceHeight);

    left = qMax(0, left); right = qMax(0, right);
    top = qMax(0, top); bottom = qMax(0, bottom);

    g_object_set(cropper, "top", top, "bottom", bottom, "left", left, "right", right, nullptr);
}

// [이동] 화면 패닝 (드래그)
void VideoWidget::panView(qreal dx, qreal dy)
{
    // 전체 화면 상태면 이동 불가
    if (currentCropRect.width() >= 1.0 && currentCropRect.height() >= 1.0) return;

    // 이동 비율 계산
    qreal percentX = dx / (qreal)this->width();
    qreal percentY = dy / (qreal)this->height();

    // 반대 방향으로 이동 (종이를 당기는 원리)
    qreal newX = currentCropRect.x() - percentX;
    qreal newY = currentCropRect.y() - percentY;

    // 화면 밖으로 나가지 않게 제한 (Clamping)
    newX = qBound(0.0, newX, 1.0 - currentCropRect.width());
    newY = qBound(0.0, newY, 1.0 - currentCropRect.height());

    applyCrop(QRectF(newX, newY, currentCropRect.width(), currentCropRect.height()));
}

void VideoWidget::resetCrop()
{
    applyCrop(QRectF(0, 0, 1, 1));
}

void VideoWidget::startRetryTimer()
{
    if (m_isPlaying || retryTimer->isActive() || currentUrl.isEmpty()) return;
    placeholderLabel->setText("Retrying..."); placeholderLabel->show();
    retryTimer->start(3000);
}

void VideoWidget::playUrl(const QString &url, int latency)
{
    if (m_isPlaying && currentUrl == url && currentLatency == latency) return;

    m_isPlaying = false;
    currentUrl = url;
    currentLatency = latency;

    if (retryTimer->isActive()) retryTimer->stop();
    if (watchdogTimer->isActive()) watchdogTimer->stop();
    if (busTimer->isActive()) busTimer->stop();
    stop();

    placeholderLabel->setText("Connecting...");
    placeholderLabel->show();

    WId winId = this->winId();
    if (winId == 0) { startRetryTimer(); return; }

    // VLC급 속도 설정 (latency=0 일 때)
    QString options = "";
    QString sinkOptions = "";
    if (latency == 0) {
        options = "drop-on-latency=true buffer-mode=none";
        sinkOptions = "sync=false";
    }

    // 파이프라인 (videocrop 포함)
    QString pipelineStr = QString(
                              "rtspsrc location=%1 protocols=tcp latency=%2 %3 ! "
                              "rtph264depay ! h264parse ! avdec_h264 ! "
                              "videoconvert ! videocrop name=crop ! videoconvert ! "
                              "autovideosink name=sink %4"
                              ).arg(url).arg(latency).arg(options).arg(sinkOptions);

    GError *error = nullptr;
    pipeline = gst_parse_launch(pipelineStr.toUtf8().constData(), &error);

    if (error || !pipeline) {
        if (error) g_error_free(error);
        startRetryTimer();
        return;
    }

    cropper = gst_bin_get_by_name(GST_BIN(pipeline), "crop");

    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler)busSyncHandler, this, nullptr);
    gst_object_unref(bus);

    busTimer->start(50);
    watchdogTimer->start(10000);
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
}

void VideoWidget::stop()
{
    m_isPlaying = false;
    if (cropper) { gst_object_unref(cropper); cropper = nullptr; }
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_element_get_state(pipeline, NULL, NULL, 500 * GST_MSECOND);
        gst_object_unref(pipeline);
        pipeline = nullptr;
    }
    placeholderLabel->show();
    placeholderLabel->setText("No Signal");
}

void VideoWidget::pollGstBus()
{
    if (!pipeline) return;
    GstBus *bus = gst_element_get_bus(pipeline);
    GstMessage *msg;
    while ((msg = gst_bus_pop(bus))) {
        switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_STATE_CHANGED:
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)) {
                GstState old, new_st;
                gst_message_parse_state_changed(msg, &old, &new_st, 0);
                if (new_st == GST_STATE_PLAYING) {
                    m_isPlaying = true; watchdogTimer->stop(); retryTimer->stop(); placeholderLabel->hide();
                }
            } break;
        case GST_MESSAGE_ERROR:
        case GST_MESSAGE_EOS:
            m_isPlaying = false; startRetryTimer(); break;
        }
        gst_message_unref(msg);
    }
    gst_object_unref(bus);
}

void VideoWidget::showEvent(QShowEvent *e) { QWidget::showEvent(e); }
void VideoWidget::resizeEvent(QResizeEvent *e) { QWidget::resizeEvent(e); if (pipeline) { /* overlay expose */ } }

// [수정] 변수명 msg로 통일
GstBusSyncReply VideoWidget::busSyncHandler(GstBus *bus, GstMessage *msg, gpointer user_data) {
    VideoWidget *widget = static_cast<VideoWidget*>(user_data);
    if (gst_is_video_overlay_prepare_window_handle_message(msg)) {
        WId winId = widget->winId();
        if (winId != 0) {
            GstVideoOverlay *overlay = GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(msg));
            gst_video_overlay_set_window_handle(overlay, (guintptr)winId);
            QMetaObject::invokeMethod(widget->placeholderLabel, "hide", Qt::QueuedConnection);
        }
        gst_message_unref(msg);
        return GST_BUS_DROP;
    }
    return GST_BUS_PASS;
}
