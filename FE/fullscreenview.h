#ifndef FULLSCREENVIEW_H
#define FULLSCREENVIEW_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QStack>
// [수정] QRubberBand 대신 QWidget 사용 (커스텀 스타일링을 위해)
// #include <QRubberBand>
#include "videowidget.h"
#include "full_underbar.h"

class FullScreenView : public QWidget
{
    Q_OBJECT
public:
    explicit FullScreenView(QWidget *parent = nullptr);
    void play(const QString &url, int index);
    void stop();
    void updateTheme(bool isDark); // Theme Support


signals:
    void closeRequested();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onZoomIn();
    void onZoomOut();
    void onRectZoomToggled(bool checked);
    void onResetZoom();

private:
    VideoWidget *videoWidget;
    FullUnderBar *underBar;
    
    // Top Bar Components
    QWidget *topBar;
    QLabel *titleLabel;
    QLabel *liveBadge;
    QPushButton *btnClose;

    // [수정] QRubberBand* -> QWidget* 으로 변경
    QWidget *rubberBand;

    QPoint originPoint;
    bool isDrawing;

    QPoint lastDragPos;
    bool isPanning;

    QStack<QRectF> zoomHistory;
    enum Mode { Normal, Drawing, Zoomed };
    Mode currentMode;

    QString getChannelName(int index);
    void setMode(Mode mode);
};

#endif // FULLSCREENVIEW_H
