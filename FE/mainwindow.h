/**
 * @file mainwindow.h
 * @brief 메인 윈도우 헤더. 모든 모듈의 컨트롤러 역할.
 */
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include "topbar.h"
#include "sidebar.h"
#include "liveview.h"
#include "fullscreenview.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    // 모듈 객체들
    TopBar *m_topBar;
    Sidebar *m_sidebar;
    QStackedWidget *m_centralStack; // 페이지 전환 컨테이너
    LiveView *m_livePage;
    FullScreenView *m_fullPage;
    QLabel *m_playbackPage;         // 임시 페이지
    QLabel *m_settingsPage;         // 임시 페이지

    void initUI();          // UI 초기화
    void initConnections(); // 시그널/슬롯 연결
};
#endif // MAINWINDOW_H
