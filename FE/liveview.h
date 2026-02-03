/**
 * @file liveview.h
 * @brief 4분할 CCTV + 2개 센서 화면을 표시하는 위젯 헤더
 */
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

signals:
    void requestFullScreen(int index);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    QGridLayout *gridLayout;
    VideoWidget *cctvWidgets[4];
    QLabel *sensorWidgets[2];
    bool streamStarted;

    void initCCTVStreams();
};

#endif // LIVEVIEW_H
