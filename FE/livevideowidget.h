#ifndef LIVEVIDEOWIDGET_H
#define LIVEVIDEOWIDGET_H

#include "videowidget.h"
#include <QTimer>

class LiveVideoWidget : public VideoWidget
{
    Q_OBJECT
public:
    explicit LiveVideoWidget(QWidget *parent = nullptr);
    ~LiveVideoWidget() override;

    void playUrl(const QString &url, int latency = 200) override;
    void stop() override;

    // Live specific
    Q_INVOKABLE void startRetryTimer();

protected:
    // Override to handle RTSP specific bus messages if needed
    // void onBusMessage(GstMessage *msg) override; 

private:
    QTimer *retryTimer;
    QTimer *watchdogTimer;
    QString currentUrl;
    int currentLatency;

public:
    QString getUrl() const { return currentUrl; }
};

#endif // LIVEVIDEOWIDGET_H
