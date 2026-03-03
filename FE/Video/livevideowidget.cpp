#include "livevideowidget.h"
#include <QDebug>
#include <QFile>
#include <QApplication>

LiveVideoWidget::LiveVideoWidget(QWidget *parent)
    : VideoWidget(parent)
{
    currentLatency = 200;

    retryTimer = new QTimer(this);
    retryTimer->setSingleShot(true);
    connect(retryTimer, &QTimer::timeout, [this](){
        if (!currentUrl.isEmpty()) playUrl(currentUrl, currentLatency);
    });

    watchdogTimer = new QTimer(this);
    watchdogTimer->setSingleShot(true);
    connect(watchdogTimer, &QTimer::timeout, [this](){
        if(isPlaying()) return; // Base class m_isPlaying
        qWarning() << "[LiveVideoWidget] Watchdog timeout. Forcing reconnect for:" << currentUrl;
        
        QString savedUrl = currentUrl;
        int savedLatency = currentLatency;
        
        VideoWidget::stop(); // Clean up pipeline, doesn't clear currentUrl
        
        currentUrl = savedUrl;
        currentLatency = savedLatency;
        
        startRetryTimer();
    });
}

LiveVideoWidget::~LiveVideoWidget()
{
    stop();
}

void LiveVideoWidget::playUrl(const QString &url, int latency)
{
    // Duplicate check
    // If playing, same URL, same latency, ignore
    if (isPlaying() && currentUrl == url && currentLatency == latency) return;

    VideoWidget::stop(); // Stop base pipeline if running
    // Ensure subclass cleanup
    if (retryTimer->isActive()) retryTimer->stop();
    if (watchdogTimer->isActive()) watchdogTimer->stop();

    currentUrl = url;
    currentLatency = latency;

    // Call base placeholder logic if needed, or handle here
    // But Base VideoWidget might manage placeholderLabel. 
    // We should expose placeholderLabel or a protected method.
    // For now, assuming we can access it or Base handles generic stop().
    // Actually Base::stop() sets m_isPlaying false and clears pipeline.

    showPlaceholder("Connecting...");

    WId winId = this->winId();
    if (winId == 0) { startRetryTimer(); return; }

    QString options = "";
    QString sinkOptions = "";
    if (latency == 0) {
        sinkOptions = "sync=false";
    }

    // RTSP Pipeline (Using UDP for better quality monitoring)
    QString pipelineStr = QString(
        "rtspsrc name=src location=%1 protocols=udp latency=%2 %3 ! "
        "qualitymonitor name=qmon ! "
        "rtph264depay ! h264parse ! avdec_h264 ! "
        "videoconvert ! videocrop name=crop ! videoconvert ! "
        "autovideosink name=sink %4"
    ).arg(url).arg(latency).arg(options).arg(sinkOptions);

    qDebug() << "[LiveVideoWidget] Playing Rtsp Stream:" << url;

    GError *error = nullptr;
    GstElement *pipeline = gst_parse_launch(pipelineStr.toUtf8().constData(), &error);

    if (error || !pipeline) {
        qCritical() << "[LiveVideoWidget] GST Error:" << (error ? error->message : "Unknown");
        if (error) g_error_free(error);
        startRetryTimer(); 
        return;
    }

    // Set pipeline in base class
    if (!setPipeline(pipeline)) { // We need to expose a way to set pipeline to base
        gst_object_unref(pipeline);
        return;
    }

    watchdogTimer->start(10000);
}

void LiveVideoWidget::stop()
{
    if (retryTimer->isActive()) retryTimer->stop();
    if (watchdogTimer->isActive()) watchdogTimer->stop();
    
    VideoWidget::stop(); // Base clears pipeline
    currentUrl.clear();
}

void LiveVideoWidget::startRetryTimer()
{
    if (isPlaying() || retryTimer->isActive() || currentUrl.isEmpty()) return;
    showPlaceholder("Retrying...");
    retryTimer->start(3000);
}
