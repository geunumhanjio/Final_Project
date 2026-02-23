#include "videowidget.h"
#include <QVBoxLayout>
#include <QDebug>
#include <QApplication>
#include <QMutexLocker>
#include <QFile>

VideoWidget::VideoWidget(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_StyledBackground, true);
    this->setStyleSheet("background-color: black;");
    setMinimumSize(160, 120);
    setFocusPolicy(Qt::NoFocus);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    placeholderLabel = new QLabel("No Signal", this);
    placeholderLabel->setAlignment(Qt::AlignCenter);
    placeholderLabel->setStyleSheet("color: white; font-size: 16px; font-weight: bold;");
    layout->addWidget(placeholderLabel);

    pipeline = nullptr;
    cropper = nullptr;
    m_isPlaying = false;
    sourceWidth = 0; // [Mod] Init to 0 to detect valid resolution
    sourceHeight = 0;
    currentCropRect = QRectF(0.0, 0.0, 1.0, 1.0); 

    busTimer = new QTimer(this);
    connect(busTimer, &QTimer::timeout, this, &VideoWidget::pollGstBus);
}

VideoWidget::~VideoWidget() { stop(); }

// Protected Helper to set Pipeline from Subclass
bool VideoWidget::setPipeline(GstElement *p) {
    if (pipeline) stop(); // Clear existing if any
    
    pipeline = p;
    if (!pipeline) return false;

    // Check for cropper
    cropper = gst_bin_get_by_name(GST_BIN(pipeline), "crop");
    if (!cropper) {
        qWarning() << "[VideoWidget] Warning: 'crop' element not found in pipeline";
    }

    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler)busSyncHandler, this, nullptr);
    gst_object_unref(bus);

    busTimer->start(50);
    
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        qCritical() << "[VideoWidget] Failed to set pipeline to PLAYING";
        return false;
    }
    return true;
}

void VideoWidget::stop()
{
    m_isPlaying = false;
    
    if (busTimer->isActive()) busTimer->stop();

    if (cropper) { gst_object_unref(cropper); cropper = nullptr; }
    
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_element_get_state(pipeline, NULL, NULL, 500 * GST_MSECOND);
        gst_object_unref(pipeline);
        pipeline = nullptr;
    }
    showPlaceholder("No Signal");
    
    // Reset Resolution
    sourceWidth = 0; 
    sourceHeight = 0;
}

void VideoWidget::updateSourceResolution() {
    if (!cropper) return;
    
    GstPad *pad = gst_element_get_static_pad(cropper, "sink");
    if (pad) {
        GstCaps *caps = gst_pad_get_current_caps(pad);
        if (caps) {
            GstStructure *str = gst_caps_get_structure(caps, 0);
            int w, h;
            if (gst_structure_get_int(str, "width", &w) && gst_structure_get_int(str, "height", &h)) {
                sourceWidth = w;
                sourceHeight = h;
                qDebug() << "[VideoWidget] Source resolution updated:" << w << "x" << h;
                
                // Re-apply current crop if valid
                if (currentCropRect.isValid()) {
                    // Temporarily unlock/lock not needed as we are in same thread usually
                    // But to be safe simply call applyCrop
                    applyCrop(currentCropRect);
                }
            }
            gst_caps_unref(caps);
        }
        gst_object_unref(pad);
    }
}

void VideoWidget::showPlaceholder(const QString &text) {
    placeholderLabel->setText(text);
    placeholderLabel->show();
}

void VideoWidget::applyCrop(const QRectF &rect)
{
    QMutexLocker locker(&cropMutex);
    currentCropRect = rect; 

    // Allow crop adjustment even if paused, as long as pipeline exists
    if (!cropper) {
        // Warning logged once
        return;
    }

    // [Fix] Ensure resolution is known before cropping
    if (sourceWidth <= 0 || sourceHeight <= 0) {
        updateSourceResolution();
        if (sourceWidth <= 0 || sourceHeight <= 0) {
            qWarning() << "[VideoWidget] cannot apply crop, source resolution unknown";
            return;
        }
    }

    int left = static_cast<int>(rect.left() * sourceWidth);
    int right = static_cast<int>((1.0 - rect.right()) * sourceWidth);
    int top = static_cast<int>(rect.top() * sourceHeight);
    int bottom = static_cast<int>((1.0 - rect.bottom()) * sourceHeight);

    left = qMax(0, left); right = qMax(0, right);
    top = qMax(0, top); bottom = qMax(0, bottom);

    // Debug
    // qDebug() << "Crop:" << left << right << top << bottom;

    g_object_set(cropper, "top", top, "bottom", bottom, "left", left, "right", right, nullptr);

    // [New] If paused, force frame update so user sees the zoom effect immediately
    if (!m_isPlaying) {
        refreshFrame();
    }
}

void VideoWidget::panView(qreal dx, qreal dy) {
    if (currentCropRect.width() >= 1.0 && currentCropRect.height() >= 1.0) return;

    qreal percentX = dx / (qreal)this->width();
    qreal percentY = dy / (qreal)this->height();

    qreal newX = currentCropRect.x() - percentX;
    qreal newY = currentCropRect.y() - percentY;

    newX = qBound(0.0, newX, 1.0 - currentCropRect.width());
    newY = qBound(0.0, newY, 1.0 - currentCropRect.height());

    applyCrop(QRectF(newX, newY, currentCropRect.width(), currentCropRect.height()));
}

void VideoWidget::resetCrop() {
    applyCrop(QRectF(0, 0, 1, 1));
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
                    m_isPlaying = true; 
                    placeholderLabel->hide();
                    updateSourceResolution(); // [New] Get correct resolution
                    emit playbackStateChanged(true);
                }
            } break;
        case GST_MESSAGE_ERROR:
        case GST_MESSAGE_EOS:
            m_isPlaying = false; 
            startRetryTimer(); // Calls virtual method (overridden by Live/Recorded)
            break;
        }
        gst_message_unref(msg);
    }
    gst_object_unref(bus);
}

void VideoWidget::showEvent(QShowEvent *e) { QWidget::showEvent(e); }
void VideoWidget::resizeEvent(QResizeEvent *e) { QWidget::resizeEvent(e); if (pipeline) { /* overlay expose */ } }

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
