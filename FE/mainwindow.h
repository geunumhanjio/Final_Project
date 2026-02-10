#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QCloseEvent>
#include "topbar.h"
#include "sidebar.h"
#include "liveview.h"
#include "fullscreenview.h"

QT_BEGIN_NAMESPACE
QT_END_NAMESPACE

class SettingsWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    TopBar *m_topBar;
    Sidebar *m_sidebar;
    QStackedWidget *m_centralStack; // 페이지 전환 컨테이너
    LiveView *m_livePage;
    FullScreenView *m_fullPage;
    QLabel *m_playbackPage;         // 임시 페이지
    SettingsWidget *m_settingsPage; // Settings Widget

    void initUI();          // UI 초기화
    void initConnections(); // 시그널/슬롯 연결
    void toggleTheme();     // Theme toggle method
    void loadTheme(const QString &path); // Robust theme loader

    
private:
    bool m_isDark; // Current theme state
};
#endif // MAINWINDOW_H
