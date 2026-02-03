#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QResizeEvent>
#include <QTimer>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>

class VideoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget();

    void playUrl(const QString &url);
    void stop();

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    QPaintEngine *paintEngine() const override { return nullptr; }

private:
    GstElement *pipeline;
    QString currentUrl;
    QLabel *placeholderLabel;
    int pipelineAttempt;
    QTimer *connectionTimeout;

    static GstBusSyncReply busSyncHandler(GstBus *bus, GstMessage *message, gpointer user_data);
    static gboolean busCallback(GstBus *bus, GstMessage *msg, gpointer data);
};

#endif // VIDEOWIDGET_H
