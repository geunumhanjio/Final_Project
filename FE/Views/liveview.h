#ifndef LIVEVIEW_H
#define LIVEVIEW_H

#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QEvent>
#include <QShowEvent>
#include <QJsonObject>
#include "videocard.h"
#include "slammapwidget.h"

class LiveView : public QWidget
{
    Q_OBJECT
public:
    explicit LiveView(QWidget *parent = nullptr);
    void setChannelVisible(int index, bool visible);
    void stopAll();

signals:
    void requestFullScreen(int index, QString url);
    void recordCommandRequested(int channelId, bool start);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override; // [Fix] Focus on click

public slots:
    void updateMap(const QJsonObject &data);
    void updateOdom(const QJsonObject &data);
    void updatePath(const QJsonObject &data);

private:
    QGridLayout *gridLayout;
    VideoCard *cctvWidgets[4]; 
    QWidget *rightPanel;       
    
    // Right Panel Widgets
    VideoCard *rcCarCamWidget;
    SlamMapWidget *slamMapWidget;

    bool streamStarted;

    QStringList lowQualityUrls;
    QStringList highQualityUrls;

    void initCCTVStreams();
    void updateCCTVLayout(); // Dynamic layout update

private slots:
    void refreshStreams();
};

#endif // LIVEVIEW_H
