#include "videowidget.h"
#include <QVBoxLayout>
#include <QDebug>
#include <QApplication>
#include <QMutexLocker>
#include "Gst/GstQualityMonitor.hpp"
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

    // [New] OSD 위젯 생성 (비디오 컨테이너의 자식으로)
    m_osdWidget = new OsdWidget(this);
    m_osdWidget->hide(); // 초기엔 숨김 처리 안함(비어있으면 안그려짐), 필요시 토글

    m_pinger = new RtspPinger(this); // [New]

    // [New] 글로벌 애플리케이션 포커스 상태 변경 감지
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive) {
            if (m_osdWidget) m_osdWidget->hide();
        } else {
            // 앱이 다시 활성화되었을 때, 이 위젯이 보이고 최소화 상태가 아닐 때만 OSD 표시
            if (m_osdWidget && this->isVisible() && !this->window()->isMinimized()) {
            if (m_osdWidget && this->isVisible()) {
                bool anyVisible = false;
                for (int i = 0; i < OsdWidget::MetricCount; ++i) {
                    if (m_osdWidget->isMetricVisible(static_cast<OsdWidget::Metric>(i))) {
                        anyVisible = true;
                        break;
                    }
                }
                if (anyVisible) m_osdWidget->show();
                syncOverlayPosition();
            }
        }
    });

                
                QPoint globalPos = this->mapToGlobal(QPoint(0, 0));
                m_osdWidget->setGeometry(globalPos.x(), globalPos.y(), this->width(), this->height());
            }
        }
    });

    // [New] Timer to constantly sync OSD position when app moves
    m_syncTimer = new QTimer(this);
    connect(m_syncTimer, &QTimer::timeout, this, &VideoWidget::syncOverlayPosition);
    m_syncTimer->start(16); // ~60fps sync rate

    // [New] Timer to extract GStreamer stats (packet loss, jitter)
    m_statsTimer = new QTimer(this);
    connect(m_statsTimer, &QTimer::timeout, this, &VideoWidget::extractGstStats);
    m_statsTimer->start(1000); // 1Hz update

    m_statsClock.start(); // Start the clock for rate measurements

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

    // [New] Start Pinger for RTT measurement
    GstElement *src = gst_bin_get_by_name(GST_BIN(pipeline), "src");
    if (src) {
        gchar *location = nullptr;
        g_object_get(src, "location", &location, NULL);
        if (location) {
            m_pinger->startPinger(QString::fromUtf8(location));
            g_free(location);
        }
        gst_object_unref(src);
    }

    // [New] Attach Pad Probe to Sink to count actual rendered frames
    GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    if (sink) {
        GstPad *sinkpad = gst_element_get_static_pad(sink, "sink");
        if (sinkpad) {
            gst_pad_add_probe(sinkpad, GST_PAD_PROBE_TYPE_BUFFER, sinkPadProbe, this, nullptr);
            gst_object_unref(sinkpad);
        }
        gst_object_unref(sink);
    }

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
    
    if (m_pinger) m_pinger->stop(); // [New] Stop pinger when stream stops

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

void VideoWidget::showEvent(QShowEvent *e) { 
    QWidget::showEvent(e); 
    if (m_osdWidget) {
        // 부모(영상)가 다시 나타나면 OSD 위젯 내부에 활성화된 지표가 1개라도 있는지 확인
        bool anyVisible = false;
        for (int i = 0; i < OsdWidget::MetricCount; ++i) {
            if (m_osdWidget->isMetricVisible(static_cast<OsdWidget::Metric>(i))) {
                anyVisible = true;
                break;
            }
        }
        if (anyVisible) m_osdWidget->show();
        
        QPoint globalPos = mapToGlobal(QPoint(0, 0));
        m_osdWidget->setGeometry(globalPos.x(), globalPos.y(), width(), height());
    }
}

void VideoWidget::hideEvent(QHideEvent *e) {
    QWidget::hideEvent(e);
    // 부모(영상)이 숨겨지거나 가려지면 (탭 전환 등) 최상단 OSD도 같이 숨기기
    if (m_osdWidget) {
        m_osdWidget->hide();
    }
}

void VideoWidget::moveEvent(QMoveEvent *e) {
    QWidget::moveEvent(e);
    // 창을 드래그해서 움직일 때 OSD 좌표 동기화
    if (m_osdWidget && m_osdWidget->isVisible()) {
        QPoint globalPos = mapToGlobal(QPoint(0, 0));
        m_osdWidget->setGeometry(globalPos.x(), globalPos.y(), width(), height());
    }
}

void VideoWidget::syncOverlayPosition() {
    if (m_osdWidget && m_osdWidget->isVisible() && this->isVisible()) {
        QPoint globalPos = mapToGlobal(QPoint(0, 0));
        // Only update if moved to save performance
        QRect currentRect = m_osdWidget->geometry();
        QRect newRect(globalPos.x(), globalPos.y(), width(), height());
        if (currentRect != newRect) {
            m_osdWidget->setGeometry(newRect);
        }
    }
}

void VideoWidget::extractGstStats() {
    if (!pipeline || !m_isPlaying || !m_osdWidget) return;

    // We only care if OSD metrics for Loss or Jitter are visible (Optimization)
    if (!m_osdWidget->isMetricVisible(OsdWidget::PacketLoss) && 
        !m_osdWidget->isMetricVisible(OsdWidget::Jitter)) {
        return;
    }

    GstIterator* it = gst_bin_iterate_recurse(GST_BIN(pipeline));
    GValue item = G_VALUE_INIT;
    GstElement* jbuf = nullptr;
    
    while (gst_iterator_next(it, &item) == GST_ITERATOR_OK) {
        GstElement* elem = GST_ELEMENT(g_value_get_object(&item));
        if (elem && g_str_has_prefix(GST_ELEMENT_NAME(elem), "rtpjitterbuffer")) {
            jbuf = elem;
            gst_object_ref(jbuf); // Ref it before we break
            g_value_reset(&item);
            break;
        }
        g_value_reset(&item);
    }
    gst_iterator_free(it);

    if (jbuf) {
        GstStructure* stats = nullptr;
        g_object_get(jbuf, "stats", &stats, NULL);

        if (stats) {
            guint64 num_lost = 0, num_pushed = 0;
            guint64 avg_jitter = 0; 

            gst_structure_get_uint64(stats, "num-lost",   &num_lost);
            gst_structure_get_uint64(stats, "num-pushed", &num_pushed);

            // Packet Loss
            double loss_rate = 0.0;
            if (num_lost + num_pushed > 0) {
                loss_rate = (double)num_lost / (num_lost + num_pushed) * 100.0;
            }
            m_osdWidget->setMetricValue(OsdWidget::PacketLoss, QString::number(loss_rate, 'f', 2) + " %");

            // Jitter
            double jitter_ms = 0.0;
            if (gst_structure_get_uint64(stats, "avg-jitter", &avg_jitter)) {
                // GStreamer 1.16+ (nanoseconds)
                jitter_ms = (double)avg_jitter / 1000000.0; 
            } else {
                // Fallback for GStreamer < 1.18
                guint32 jitter_rtp = 0;
                if (gst_structure_get_uint(stats, "jitter", &jitter_rtp)) {
                    jitter_ms = (double)jitter_rtp / 90000.0 * 1000.0; // Assuming 90kHz H.264
                }
            }
            m_osdWidget->setMetricValue(OsdWidget::Jitter, QString::number(jitter_ms, 'f', 2) + " ms");

            gst_structure_free(stats);
        }
        gst_object_unref(jbuf);
    }
}

void VideoWidget::resizeEvent(QResizeEvent *e) { 
    QWidget::resizeEvent(e); 
    if (pipeline) { /* overlay expose */ } 
    // [New] OSD 위젯의 크기를 부모(비디오)에 맞춤 - 탑레벨이므로 Global 좌표 사용
    if (m_osdWidget) {
        QPoint globalPos = mapToGlobal(QPoint(0, 0));
        m_osdWidget->setGeometry(globalPos.x(), globalPos.y(), width(), height());
    }
}

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
