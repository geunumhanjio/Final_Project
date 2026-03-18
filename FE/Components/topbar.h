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
#include <QMouseEvent>

class TopBar : public QWidget
{
    Q_OBJECT
public:
    explicit TopBar(QWidget *parent = nullptr);
 // Sync Text/Icon color

signals:
    void sidebarToggled();       // Sidebar toggle signal
    void modeChanged(int index); // Mode change signal
    void themeToggled();         // Theme toggle signal
    void closeRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

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
    QPushButton *btnEmergencyStop;
    QPushButton *btnTheme;
    QPushButton *btnClose;
    QLabel *userIcon;
    QLabel *titleLabel;
    
    QTimer *timer;
    bool m_isDraggingWindow = false;
    QPoint m_windowDragOffset;
};

#endif // TOPBAR_H
