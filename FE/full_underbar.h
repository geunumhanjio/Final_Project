#ifndef FULL_UNDERBAR_H
#define FULL_UNDERBAR_H

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFrame>

class FullUnderBar : public QWidget
{
    Q_OBJECT
public:
    explicit FullUnderBar(QWidget *parent = nullptr);
    // 버튼 상태 변경 (0:기본, 1:그리기중, 2:확대됨)
    void setRectButtonMode(int state);
    void updateTheme(bool isDark); // Theme switcher

protected:
    void paintEvent(QPaintEvent *event) override;

signals:
    void reqZoomIn();
    void reqZoomOut();
    void reqRectZoom(bool checked);
    void reqResetZoom(); // [신규] 초기화 버튼

private:
    QPushButton *btnZoomIn;
    QPushButton *btnZoomOut;
    QPushButton *btnRectZoom;
    QPushButton *btnResetZoom; // [신규]

    QFrame* createDivider(); // Helper
};

#endif // FULL_UNDERBAR_H
