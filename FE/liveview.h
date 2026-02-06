#ifndef LIVEVIEW_H
#define LIVEVIEW_H

#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QEvent>
#include <QShowEvent>
#include "videowidget.h"

class LiveView : public QWidget
{
    Q_OBJECT
public:
    explicit LiveView(QWidget *parent = nullptr);
    void setChannelVisible(int index, bool visible);
    void stopAll();

signals:
    void requestFullScreen(int index, QString url);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    QGridLayout *gridLayout;
    VideoWidget *cctvWidgets[4];
    QLabel *sensorWidgets[2];
    bool streamStarted;

    QStringList lowQualityUrls;
    QStringList highQualityUrls;

    void initCCTVStreams();
};

#endif // LIVEVIEW_H
