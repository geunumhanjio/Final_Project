/**
 * @file topbar.h
 * @brief 상단 네비게이션 바 헤더
 */
#ifndef TOPBAR_H
#define TOPBAR_H

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>

class TopBar : public QWidget
{
    Q_OBJECT
public:
    explicit TopBar(QWidget *parent = nullptr);

signals:
    void sidebarToggled();       // 사이드바 토글 시그널
    void modeChanged(int index); // 화면 모드 변경 시그널 (0:라이브, 1:기록, 2:설정)

private:
    QHBoxLayout *layout;
    QPushButton *btnToggle;
    QPushButton *btnLive;
    QPushButton *btnPlayback;
    QPushButton *btnSettings;
};

#endif // TOPBAR_H
