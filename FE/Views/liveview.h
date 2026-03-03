#ifndef LIVEVIEW_H
#define LIVEVIEW_H

#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QEvent>
#include <QShowEvent>
#include "videocard.h"

class LiveView : public QWidget
{
    Q_OBJECT
public:
    explicit LiveView(QWidget *parent = nullptr);
    void setChannelVisible(int index, bool visible);
    void stopAll();
    void updateStreamStats(int channelId, double fps, double bitrateKbps, double proxyLatencyMs); // [New]

signals:
    void requestFullScreen(int index, QString url);
    void recordCommandRequested(int channelId, bool start);
    void streamStatsRequested(int channelId, bool start); // [New]

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override; // [Fix] Focus on click

private:
    QGridLayout *gridLayout;
    VideoCard *cctvWidgets[4]; 
    QWidget *rightPanel;       
    
    // Right Panel Widgets
    VideoCard *rcCarCamWidget;
    QLabel *slamMapWidget;

    bool streamStarted;

    QStringList lowQualityUrls;
    QStringList highQualityUrls;

    void initCCTVStreams();
    void updateCCTVLayout(); // Dynamic layout update

private slots:
    void refreshStreams();
};

#endif // LIVEVIEW_H
