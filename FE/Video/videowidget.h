#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <QMutex>
#include <QRectF>
#include <QElapsedTimer>
#include "osdwidget.h"
#include "rtsppinger.h" // [New]

class VideoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoWidget(QWidget *parent = nullptr);
    virtual ~VideoWidget();

    virtual void playUrl(const QString &url, int latency = 200) = 0; // Pure virtual
    virtual void stop();
    Q_INVOKABLE virtual void startRetryTimer() {}

    bool isPlaying() const { return m_isPlaying; }

    virtual void pause() {}
    virtual void resume() {}
    virtual void seek(qint64 positionMs) {}
    virtual void seekRelative(qint64 offsetMs) {}
    virtual qint64 getDuration() { return 0; }
    virtual qint64 getPosition() { return 0; }

    void applyCrop(const QRectF &rect);
    void resetCrop();
    void panView(qreal dx, qreal dy);
    QRectF getCurrentCrop() const { return currentCropRect; }
    QRectF getVideoDisplayRect() const;
    QPointF widgetPointToVideoNormalized(const QPointF &widgetPoint, bool *ok = nullptr) const;
    QPointF videoNormalizedToWidgetPoint(const QPointF &normalizedPoint) const;

    virtual void refreshFrame() {} // [New] For forcing update when paused

    // [New] OSD 위젯 접근 (메뉴 연동용)
    OsdWidget* getOsdWidget() const { return m_osdWidget; }

signals:
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void playbackStateChanged(bool isPlaying);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override; // [New]
    void moveEvent(QMoveEvent *event) override; // [New]
    void resizeEvent(QResizeEvent *event) override;
    QPaintEngine *paintEngine() const override { return nullptr; }

    // [New] Common stream state
    QString currentUrl;
    int currentLatency;

    // Helpers for subclasses
    bool setPipeline(GstElement *p);
    GstElement* getPipeline() const { return pipeline; }
    void showPlaceholder(const QString &text);
    void setPlayingState(bool playing) { m_isPlaying = playing; }
    void updateSourceResolution(); // [New]

protected slots:
    virtual void pollGstBus();

private:
    GstElement *pipeline;
    GstElement *cropper;
    int sourceWidth, sourceHeight;
    int sourcePixelAspectNum = 1;
    int sourcePixelAspectDen = 1;
    mutable QMutex cropMutex;
    QRectF currentCropRect;

    QLabel *placeholderLabel;
    QTimer *busTimer;

    bool m_isPlaying;

    // [New] OSD Widget
    OsdWidget *m_osdWidget;
    RtspPinger *m_pinger; // [New]
    void syncOverlayPosition(); // [New]

    QTimer *m_statsTimer; // [New] For pulling GST stats
    void extractGstStats(); // [New]

    uint64_t m_lastBytes = 0;
    uint64_t m_lastPackets = 0;
    int32_t  m_lastLost = 0;
    uint64_t m_lastFrames = 0;
    uint64_t m_actualFrameCount = 0; // [New] Actual rendered frame count
    guint m_lastRendered = 0;
    QElapsedTimer m_statsClock;

    static GstBusSyncReply busSyncHandler(GstBus *bus, GstMessage *msg, gpointer user_data);
    static GstPadProbeReturn sinkPadProbe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data); // [New]
};

#endif // VIDEOWIDGET_H
