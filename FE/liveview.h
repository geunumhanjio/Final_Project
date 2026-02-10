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

signals:
    void requestFullScreen(int index, QString url);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

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
