#ifndef RECORDEDVIDEOWIDGET_H
#define RECORDEDVIDEOWIDGET_H

#include "videowidget.h"
#include <QTimer>

class RecordedVideoWidget : public VideoWidget
{
    Q_OBJECT
public:
    explicit RecordedVideoWidget(QWidget *parent = nullptr);
    ~RecordedVideoWidget() override;

    void playUrl(const QString &url, int latency = 0) override;
    void stop() override;

    void pause() override;
    void resume() override;
    void seek(qint64 positionMs) override;
    void seekRelative(qint64 offsetMs) override; // [Fix] Added missing declaration
    qint64 getDuration() override;
    qint64 getPosition() override;

    void refreshFrame() override; // [New]

private slots:
    void updatePosition();

private:
    QTimer *positionTimer;
    gint64 m_durationMs;
    QString currentUrl;
};

#endif // RECORDEDVIDEOWIDGET_H
