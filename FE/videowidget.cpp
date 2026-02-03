#include "videowidget.h"
#include <QVBoxLayout>
#include <QDebug>
#include <QApplication>

VideoWidget::VideoWidget(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);

    this->setStyleSheet("background-color: black;");
    setMinimumSize(160, 120);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    placeholderLabel = new QLabel("No Signal", this);
    placeholderLabel->setAlignment(Qt::AlignCenter);
    placeholderLabel->setStyleSheet("color: white; font-size: 16px; font-weight: bold;");
    layout->addWidget(placeholderLabel);

    pipeline = nullptr;
    pipelineAttempt = 0;
    connectionTimeout = nullptr;
}

VideoWidget::~VideoWidget()
{
    stop();
}

void VideoWidget::playUrl(const QString &url)
{
    stop();
    currentUrl = url;

    placeholderLabel->setText("Connecting...");
    placeholderLabel->show();

    qDebug() << "[VideoWidget] ==========================================";
    qDebug() << "[VideoWidget] Connecting to:" << url;

    // WinId 강제 생성
    WId winId = this->winId();
    qDebug() << "[VideoWidget] WinId:" << winId;

    if (winId == 0) {
        qCritical() << "[VideoWidget] ERROR: WinId is 0!";
        placeholderLabel->setText("Widget Error");
        return;
    }

    // CMD에서 작동한 단순한 파이프라인 사용
    QString pipelineStr = QString(
                              "rtspsrc location=%1 protocols=tcp latency=200 ! "
                              "decodebin ! "
                              "videoconvert ! "
                              "autovideosink name=sink"
                              ).arg(url);

    qDebug() << "[VideoWidget] Pipeline:" << pipelineStr;

    GError *error = nullptr;
    pipeline = gst_parse_launch(pipelineStr.toUtf8().constData(), &error);

    if (error) {
        qCritical() << "[VideoWidget] Pipeline error:" << error->message;
        placeholderLabel->setText(QString("Error:\n%1").arg(error->message));
        g_error_free(error);
        return;
    }

    if (!pipeline) {
        qCritical() << "[VideoWidget] Pipeline is NULL";
        placeholderLabel->setText("Pipeline Error");
        return;
    }

    // 메시지 버스 설정
    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler)busSyncHandler, this, nullptr);
    gst_bus_add_watch(bus, (GstBusFunc)busCallback, this);
    gst_object_unref(bus);

    // 재생 시작
    qDebug() << "[VideoWidget] Starting playback...";
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);

    if (ret == GST_STATE_CHANGE_FAILURE) {
        qCritical() << "[VideoWidget] Failed to start";
        placeholderLabel->setText("Connection Failed");
        stop();
        return;
    }

    qDebug() << "[VideoWidget] State change:" << ret;
    qDebug() << "[VideoWidget] ==========================================";
}

void VideoWidget::stop()
{
    if (pipeline) {
        qDebug() << "[VideoWidget] Stopping pipeline...";
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        pipeline = nullptr;
    }

    if (placeholderLabel) {
        placeholderLabel->show();
        placeholderLabel->setText("No Signal");
    }
}

void VideoWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    qDebug() << "[VideoWidget] showEvent - WinId:" << this->winId();
}

void VideoWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (pipeline) {
        GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
        if (sink && GST_IS_VIDEO_OVERLAY(sink)) {
            gst_video_overlay_expose(GST_VIDEO_OVERLAY(sink));
            gst_object_unref(sink);
        }
    }
}

GstBusSyncReply VideoWidget::busSyncHandler(GstBus *bus, GstMessage *message, gpointer user_data)
{
    VideoWidget *widget = static_cast<VideoWidget*>(user_data);

    if (gst_is_video_overlay_prepare_window_handle_message(message)) {
        WId winId = widget->winId();

        qDebug() << "[VideoWidget] prepare-window-handle, WinId:" << winId;

        if (winId != 0) {
            GstVideoOverlay *overlay = GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(message));
            gst_video_overlay_set_window_handle(overlay, (guintptr)winId);
            qDebug() << "[VideoWidget] Window handle set";

            if (widget->placeholderLabel) {
                widget->placeholderLabel->hide();
            }
        }

        gst_message_unref(message);
        return GST_BUS_DROP;
    }

    return GST_BUS_PASS;
}

gboolean VideoWidget::busCallback(GstBus *bus, GstMessage *msg, gpointer data)
{
    VideoWidget *widget = static_cast<VideoWidget*>(data);

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
        GError *err;
        gchar *debug;
        gst_message_parse_error(msg, &err, &debug);

        qCritical() << "[VideoWidget] ERROR:" << err->message;
        if (debug) qCritical() << "[VideoWidget] Debug:" << debug;

        if (widget && widget->placeholderLabel) {
            widget->placeholderLabel->setText(QString("Error:\n%1").arg(err->message));
            widget->placeholderLabel->show();
        }

        g_error_free(err);
        g_free(debug);
        break;
    }
    case GST_MESSAGE_STATE_CHANGED: {
        if (widget && GST_MESSAGE_SRC(msg) == GST_OBJECT(widget->pipeline)) {
            GstState old_state, new_state;
            gst_message_parse_state_changed(msg, &old_state, &new_state, nullptr);
            qDebug() << "[VideoWidget]"
                     << gst_element_state_get_name(old_state)
                     << "->" << gst_element_state_get_name(new_state);

            if (new_state == GST_STATE_PLAYING && widget->placeholderLabel) {
                qDebug() << "[VideoWidget] NOW PLAYING";
                widget->placeholderLabel->hide();
            }
        }
        break;
    }
    case GST_MESSAGE_STREAM_START: {
        qDebug() << "[VideoWidget] STREAM STARTED";
        break;
    }
    default:
        break;
    }

    return TRUE;
}
