/**
 * @file topbar.cpp
 * @brief Top navigation bar implementation
 */
#include "topbar.h"
#include <QStyle>
#include <QAbstractButton>
#include <QMenu>

// Helper to update style
void updateStyle(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}

TopBar::TopBar(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    this->setFixedHeight(56);
    this->setObjectName("TopBar"); // For QSS

    setupUi();

    // Timer for clock
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &TopBar::updateTime);
    timer->start(1000);
    timer->start(1000);
    updateTime();

}

void TopBar::setupUi()
{
    layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->setSpacing(12);

    // 1. Sidebar Toggle & Title
    btnToggle = new QPushButton(this);
    btnToggle->setText("☰");
    btnToggle->setFixedSize(32, 32);
    btnToggle->setCursor(Qt::PointingHandCursor);
    btnToggle->setObjectName("TopBarToggleBtn"); // [New]
    btnToggle->setStyleSheet("QPushButton { color: #94a3b8; font-size: 20px; border: none; background: transparent; } QPushButton:hover { color: white; background: rgba(255,255,255,0.05); border-radius: 4px; }");
    // Note: btnToggle icon color might need manual update or QIcon theme, keeping simple for now.

    titleLabel = new QLabel("Monitoring System", this);
    titleLabel->setObjectName("TopBarTitle");
    // Initial Style set in updateTheme 
    // titleLabel->setStyleSheet("font-weight: bold; font-size: 16px; letter-spacing: 1px; color: #e2e8f0; background-color: transparent;");

    // 2. Navigation Tabs
    QWidget *navContainer = new QWidget(this);
    navContainer->setObjectName("TopBarNavContainer");
    QHBoxLayout *navLayout = new QHBoxLayout(navContainer);
    navLayout->setContentsMargins(4, 4, 4, 4);
    navLayout->setSpacing(4);

    btnLive = new QPushButton("● Live", this);
    btnPlayback = new QPushButton("Playback", this);
    btnSettings = new QPushButton("Settings", this);
    
    btnLive->setObjectName("TopBarNavBtn");
    btnPlayback->setObjectName("TopBarNavBtn");
    btnSettings->setObjectName("TopBarNavBtn");
    
    // Set initial active state
    btnLive->setProperty("active", true);
    btnPlayback->setProperty("active", false);
    btnSettings->setProperty("active", false);
    
    navLayout->addWidget(btnLive);
    navLayout->addWidget(btnPlayback);
    navLayout->addWidget(btnSettings);

    // 3. Right Side (Time, Theme, User)
    QWidget *rightContainer = new QWidget(this);
    rightContainer->setStyleSheet("background: transparent;");
    QVBoxLayout *timeLayout = new QVBoxLayout(rightContainer);
    timeLayout->setContentsMargins(0, 0, 0, 0);
    timeLayout->setSpacing(0);
    timeLayout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    
    dateLabel = new QLabel(this);
    dateLabel->setObjectName("DateLabel"); // Styled in QSS if needed, or inline
    dateLabel->setStyleSheet("color: #64748b; font-family: monospace; font-size: 11px; font-weight: bold; background-color: transparent;");
    dateLabel->setAlignment(Qt::AlignRight);

    timeLabel = new QLabel(this);
    timeLabel->setObjectName("TimeLabel");
    timeLabel->setStyleSheet("color: #135bec; font-family: monospace; font-weight: bold; font-size: 14px; background-color: transparent;");
    timeLabel->setAlignment(Qt::AlignRight);
    
    timeLayout->addWidget(dateLabel);
    timeLayout->addWidget(timeLabel);

    btnClose = new QPushButton("X", this);
    btnClose->setFixedSize(36, 36);
    btnClose->setCursor(Qt::PointingHandCursor);
    btnClose->setObjectName("TopBarCloseBtn");
    btnClose->setStyleSheet(
        "QPushButton { color: #f8fafc; font-size: 14px; font-weight: bold; border: none; "
        "background: rgba(239, 68, 68, 0.88); border-radius: 10px; padding-bottom: 1px; }"
        "QPushButton:hover { background: rgba(220, 38, 38, 0.96); }"
        "QPushButton:pressed { background: rgba(185, 28, 28, 1.0); }");

    btnTheme = new QPushButton("☀", this); 
    btnTheme->setFixedSize(32, 32);
    btnTheme->setObjectName("ThemeBtn");
    btnTheme->setStyleSheet("color: #94a3b8; font-size: 18px; border:none; background:transparent;");
    
    userIcon = new QPushButton(QStringLiteral("👤"), this);
    btnEmergencyStop = new QPushButton(QStringLiteral("즉시 정지"), this);
    btnEmergencyStop->hide();
    userIcon->setFixedSize(32, 32);
    userIcon->setCursor(Qt::PointingHandCursor);
    userIcon->setObjectName("TopBarUserButton");
    userIcon->setStyleSheet(
        "QPushButton#TopBarUserButton {"
        "background-color: rgba(19, 91, 236, 0.2);"
        "color: #135bec;"
        "border: 1px solid rgba(19, 91, 236, 0.3);"
        "border-radius: 16px;"
        "font-size: 16px;"
        "}"
        "QPushButton#TopBarUserButton:hover {"
        "background-color: rgba(19, 91, 236, 0.2);"
        "border-color: rgba(19, 91, 236, 0.3);"
        "}");

    // Add to Main Layout
    layout->addWidget(btnToggle);
    layout->addSpacing(8);
    layout->addWidget(titleLabel);
    layout->addStretch(); 
    layout->addWidget(navContainer);
    layout->addStretch();
    layout->addWidget(rightContainer);
    layout->addSpacing(10);
    layout->addWidget(btnTheme);
    layout->addWidget(userIcon);
    layout->addWidget(btnClose);

    // Connections
    connect(btnToggle, &QPushButton::clicked, this, &TopBar::sidebarToggled);
    connect(btnClose, &QPushButton::clicked, this, &TopBar::closeRequested);
    
    // Theme Toggle
    connect(btnTheme, &QPushButton::clicked, [=](){
        emit themeToggled(); 
        // Icon update will happen via separate method or simple check
        if (btnTheme->text() == "☀") btnTheme->setText("🌙");
        else btnTheme->setText("☀");
    });
    
    connect(userIcon, &QPushButton::clicked, this, [this]() {
        QMenu menu(this);
        menu.setStyleSheet(
            "QMenu { background: #101827; color: #e2e8f0; border: 1px solid #334155; padding: 6px; }"
            "QMenu::item { padding: 8px 14px; border-radius: 8px; }"
            "QMenu::item:selected { background: #1d4ed8; color: white; }"
            "QMenu::separator { height: 1px; background: #334155; margin: 6px 8px; }");

        QAction *userAction = menu.addAction(QStringLiteral("User: %1").arg(m_currentUserId.isEmpty() ? QStringLiteral("Unknown") : m_currentUserId));
        userAction->setEnabled(false);

        QAction *emailAction = menu.addAction(QStringLiteral("Email: %1").arg(m_currentUserEmail.isEmpty() ? QStringLiteral("-") : m_currentUserEmail));
        emailAction->setEnabled(false);

        QAction *serverAction = menu.addAction(QStringLiteral("Server: %1").arg(m_currentServerHost.isEmpty() ? QStringLiteral("-") : m_currentServerHost));
        serverAction->setEnabled(false);

        menu.addSeparator();
        QAction *logoutAction = menu.addAction(QStringLiteral("Log Out"));
        if (menu.exec(userIcon->mapToGlobal(QPoint(0, userIcon->height()))) == logoutAction) {
            emit logoutRequested();
        }
    });

    auto updateNav = [=](QPushButton* active){
        btnLive->setProperty("active", false);
        btnPlayback->setProperty("active", false);
        btnSettings->setProperty("active", false);
        active->setProperty("active", true);
        
        updateStyle(btnLive);
        updateStyle(btnPlayback);
        updateStyle(btnSettings);
    };

    connect(btnLive, &QPushButton::clicked, [=](){ 
        emit modeChanged(0); 
        updateNav(btnLive);
    });
    connect(btnPlayback, &QPushButton::clicked, [=](){ 
        emit modeChanged(1); 
        updateNav(btnPlayback);
    });
    connect(btnSettings, &QPushButton::clicked, [=](){ 
        emit modeChanged(2); 
        updateNav(btnSettings);
    });
}

void TopBar::setCurrentUserInfo(const QString &userId, const QString &email, const QString &serverHost)
{
    m_currentUserId = userId.trimmed();
    m_currentUserEmail = email.trimmed();
    m_currentServerHost = serverHost.trimmed();

    if (!userIcon) {
        return;
    }

    userIcon->setText(QStringLiteral("👤"));
    userIcon->setToolTip(QStringLiteral("Signed in as %1").arg(m_currentUserId.isEmpty() ? QStringLiteral("Unknown") : m_currentUserId));
}

void TopBar::updateTime()
{
    // 3초 느리게 표시 (User Request)
    QDateTime now = QDateTime::currentDateTime().addSecs(-3);
    dateLabel->setText(now.toString("yyyy-MM-dd"));
    timeLabel->setText(now.toString("HH:mm:ss"));
}

void TopBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QWidget *pressedChild = childAt(event->pos());
        if (!qobject_cast<QAbstractButton *>(pressedChild) && window()) {
            m_isDraggingWindow = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            m_windowDragOffset = event->globalPosition().toPoint() - window()->frameGeometry().topLeft();
#else
            m_windowDragOffset = event->globalPos() - window()->frameGeometry().topLeft();
#endif
            event->accept();
            return;
        }
    }

    QWidget::mousePressEvent(event);
}

void TopBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDraggingWindow && (event->buttons() & Qt::LeftButton) && window()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QPoint globalPos = event->globalPosition().toPoint();
#else
        const QPoint globalPos = event->globalPos();
#endif
        window()->move(globalPos - m_windowDragOffset);
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void TopBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDraggingWindow = false;
    }

    QWidget::mouseReleaseEvent(event);
}
