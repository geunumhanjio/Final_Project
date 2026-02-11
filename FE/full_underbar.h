#ifndef FULL_UNDERBAR_H
#define FULL_UNDERBAR_H

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFrame>
#include <QSlider>
#include <QLabel>

class FullUnderBar : public QWidget
{
    Q_OBJECT
public:
    explicit FullUnderBar(QWidget *parent = nullptr);
    // 버튼 상태 변경 (0:기본, 1:그리기중, 2:확대됨)
    // 버튼 상태 변경 (0:기본, 1:그리기중, 2:확대됨)
    void setRectButtonMode(int state);
    void updateTheme(bool isDark); // Theme switcher

    // [New] Playback Controls
    void setMode(bool isFile); // True: Playback, False: Live
    void updateTime(qint64 currentMs, qint64 totalMs);
    void setPlaying(bool isPlaying);

protected:
    void paintEvent(QPaintEvent *event) override;

signals:
    void reqZoomIn();
    void reqZoomOut();
    void reqRectZoom(bool checked);
    void reqResetZoom(); // [신규] 초기화 버튼
    
    // [New] Playback Signals
    void reqPlayPause();
    void reqSeek(qint64 percent); // 0-1000 range
    void reqSkipForward();
    void reqSkipBackward();

private:
    QPushButton *btnZoomIn;
    QPushButton *btnZoomOut;
    QPushButton *btnRectZoom;
    QPushButton *btnResetZoom; // [신규]

    // [New] Playback Widgets
    QWidget *playbackContainer;
    QPushButton *btnPlayPause;
    QPushButton *btnSkipForward;
    QPushButton *btnSkipBackward;
    QSlider *seekSlider;
    QLabel *timeLabel;
    
    bool m_isFileMode;

    QFrame* createDivider(); // Helper
};

#endif // FULL_UNDERBAR_H
