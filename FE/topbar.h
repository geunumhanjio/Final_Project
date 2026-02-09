/**
 * @file topbar.h
 * @brief Top navigation bar header
 */
#ifndef TOPBAR_H
#define TOPBAR_H

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QDateTime>

class TopBar : public QWidget
{
    Q_OBJECT
public:
    explicit TopBar(QWidget *parent = nullptr);
    void updateTheme(bool isDark); // Sync Text/Icon color

signals:
    void sidebarToggled();       // Sidebar toggle signal
    void modeChanged(int index); // Mode change signal
    void themeToggled();         // Theme toggle signal

private:
    void setupUi();
    void updateTime();

    QHBoxLayout *layout;
    QPushButton *btnToggle;
    
    // Navigation
    QPushButton *btnLive;
    QPushButton *btnPlayback;
    QPushButton *btnSettings;

    // Right Side
    QLabel *timeLabel;
    QLabel *dateLabel; // Added dateLabel
    QPushButton *btnTheme;
    QLabel *userIcon;
    QLabel *titleLabel;
    
    QTimer *timer;
};

#endif // TOPBAR_H
