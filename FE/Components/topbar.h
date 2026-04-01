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
#include <QString>
#include <QStringList>

class TopBar : public QWidget
{
    Q_OBJECT
public:
    explicit TopBar(QWidget *parent = nullptr);
    void setCurrentUserInfo(const QString &userId, const QString &email, const QString &serverHost);
    void appendFallAlertLog(const QString &entry);

signals:
    void sidebarToggled();       // Sidebar toggle signal
    void modeChanged(int index); // Mode change signal
    void themeToggled();         // Theme toggle signal
    void closeRequested();
    void logoutRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void setupUi();
    void updateTime();
    void showFallAlertHistoryPopup();
    void updateAlertButtonState();

    QHBoxLayout *layout;
    QPushButton *btnToggle;
    
    // Navigation
    QPushButton *btnLive;
    QPushButton *btnPlayback;
    QPushButton *btnSettings;

    // Right Side
    QLabel *timeLabel;
    QLabel *dateLabel; // Added dateLabel
    QPushButton *btnAlert;
    QPushButton *btnEmergencyStop;
    QPushButton *btnTheme;
    QPushButton *btnClose;
    QPushButton *userIcon;
    QLabel *titleLabel;
    
    QTimer *timer;
    bool m_isDraggingWindow = false;
    QPoint m_windowDragOffset;
    QString m_currentUserId;
    QString m_currentUserEmail;
    QString m_currentServerHost;
    QStringList m_fallAlertLogs;
    bool m_hasUnreadFallAlerts = false;
};

#endif // TOPBAR_H
