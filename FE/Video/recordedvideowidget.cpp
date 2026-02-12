#include "recordedvideowidget.h"
#include <QDebug>
#include <QFile>

RecordedVideoWidget::RecordedVideoWidget(QWidget *parent)
    : VideoWidget(parent)
{
    positionTimer = new QTimer(this);
    connect(positionTimer, &QTimer::timeout, this, &RecordedVideoWidget::updatePosition);
    m_durationMs = 0;
}

RecordedVideoWidget::~RecordedVideoWidget()
{
    stop();
}

void RecordedVideoWidget::playUrl(const QString &url, int latency)
{
    if (isPlaying() && currentUrl == url) return;

    VideoWidget::stop(); // Stop base
    currentUrl = url;
    m_durationMs = 0;

    showPlaceholder("Loading File...");

    // File Pipeline
    #ifdef Q_OS_WIN
        QString path = url; 
        path.replace("\\", "/"); 
    #else
        QString path = url;
    #endif

    QString pipelineStr = QString(
        "filesrc location=\"%1\" ! decodebin ! queue ! "
        "videoconvert ! videocrop name=crop ! videoconvert ! "
        "autovideosink name=sink sync=true" 
    ).arg(path);

    qDebug() << "[RecordedVideoWidget] Playing File:" << path;

    GError *error = nullptr;
    GstElement *pipeline = gst_parse_launch(pipelineStr.toUtf8().constData(), &error);

    if (error || !pipeline) {
        qCritical() << "[RecordedVideoWidget] GST Error:" << (error ? error->message : "Unknown");
        if (error) g_error_free(error);
        return;
    }

    if (!setPipeline(pipeline)) {
        gst_object_unref(pipeline);
        return;
    }
    
    // Start position timer when playing
    positionTimer->start(500); 
}

void RecordedVideoWidget::stop()
{
    positionTimer->stop();
    m_durationMs = 0;
    emit positionChanged(0);
    emit durationChanged(0);
    emit playbackStateChanged(false);
    
    VideoWidget::stop();
    currentUrl.clear();
}

void RecordedVideoWidget::pause() {
    GstElement *pipeline = getPipeline();
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_PAUSED);
        setPlayingState(false);
        positionTimer->stop();
        emit playbackStateChanged(false);
    }
}

void RecordedVideoWidget::resume() {
    GstElement *pipeline = getPipeline();
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_PLAYING);
        setPlayingState(true);
        positionTimer->start(500); 
        emit playbackStateChanged(true);
    }
}

void RecordedVideoWidget::seek(qint64 positionMs) {
    GstElement *pipeline = getPipeline();
    if (!pipeline) return;
    
    if (!gst_element_seek_simple(pipeline, GST_FORMAT_TIME, 
        (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), 
        positionMs * GST_MSECOND)) {
        qWarning() << "[RecordedVideoWidget] Seek failed!";
    }
}

void RecordedVideoWidget::seekRelative(qint64 offsetMs) {
    qint64 current = getPosition();
    qint64 target = current + offsetMs;
    // Bounds check
    if (target < 0) target = 0;
    qint64 dur = getDuration();
    if (dur > 0 && target > dur) target = dur;

    seek(target);
}

qint64 RecordedVideoWidget::getPosition() {
    GstElement *pipeline = getPipeline();
    if (!pipeline) return 0;
    gint64 current = 0;
    if (gst_element_query_position(pipeline, GST_FORMAT_TIME, &current)) {
        return current / GST_MSECOND;
    }
    return 0;
}

qint64 RecordedVideoWidget::getDuration() {
    GstElement *pipeline = getPipeline();
    if (!pipeline) return 0;
    if (m_durationMs > 0) return m_durationMs; 

    gint64 dur = 0;
    if (gst_element_query_duration(pipeline, GST_FORMAT_TIME, &dur)) {
        m_durationMs = dur / GST_MSECOND;
        return m_durationMs;
    }
    return 0;
}

void RecordedVideoWidget::updatePosition() {
    if (!isPlaying()) return;
    
    emit positionChanged(getPosition());
    qint64 dur = getDuration();
    if (dur > 0) emit durationChanged(dur);
}
