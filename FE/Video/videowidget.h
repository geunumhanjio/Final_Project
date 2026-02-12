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

signals:
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void playbackStateChanged(bool isPlaying);

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    QPaintEngine *paintEngine() const override { return nullptr; }

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
    QMutex cropMutex;
    QRectF currentCropRect;

    QLabel *placeholderLabel;
    QTimer *busTimer;

    bool m_isPlaying;

    static GstBusSyncReply busSyncHandler(GstBus *bus, GstMessage *msg, gpointer user_data);
};

#endif // VIDEOWIDGET_H
