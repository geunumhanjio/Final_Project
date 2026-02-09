/**
 * @file topbar.cpp
 * @brief Top navigation bar implementation
 */
#include "topbar.h"
#include <QStyle>

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
    updateTheme(true); // Default Dark
}

void TopBar::setupUi()
{
    layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->setSpacing(12);

    // 1. Sidebar Toggle & Logo
    btnToggle = new QPushButton(this);
    btnToggle->setText("☰");
    btnToggle->setFixedSize(32, 32);
    btnToggle->setCursor(Qt::PointingHandCursor);
    btnToggle->setStyleSheet("QPushButton { color: #94a3b8; font-size: 20px; border: none; background: transparent; } QPushButton:hover { color: white; background: rgba(255,255,255,0.05); border-radius: 4px; }");
    // Note: btnToggle icon color might need manual update or QIcon theme, keeping simple for now.

    QLabel *logoIcon = new QLabel("📹", this);
    logoIcon->setFixedSize(32, 32);
    logoIcon->setAlignment(Qt::AlignCenter);
    logoIcon->setStyleSheet("background-color: transparent; font-size: 24px; border: none;"); 
    
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

    btnTheme = new QPushButton("☀", this); 
    btnTheme->setFixedSize(32, 32);
    btnTheme->setObjectName("ThemeBtn");
    btnTheme->setStyleSheet("color: #94a3b8; font-size: 18px; border:none; background:transparent;");
    
    userIcon = new QLabel("👤", this);
    userIcon->setFixedSize(32, 32);
    userIcon->setAlignment(Qt::AlignCenter);
    userIcon->setStyleSheet("background-color: rgba(19, 91, 236, 0.2); color: #135bec; border: 1px solid rgba(19, 91, 236, 0.3); border-radius: 16px; font-size: 16px;");

    // Add to Main Layout
    layout->addWidget(btnToggle);
    layout->addSpacing(8);
    layout->addWidget(logoIcon);
    layout->addWidget(titleLabel);
    layout->addStretch(); 
    layout->addWidget(navContainer);
    layout->addStretch();
    layout->addWidget(rightContainer);
    layout->addSpacing(10);
    layout->addWidget(btnTheme);
    layout->addWidget(userIcon);

    // Connections
    connect(btnToggle, &QPushButton::clicked, this, &TopBar::sidebarToggled);
    
    // Theme Toggle
    connect(btnTheme, &QPushButton::clicked, [=](){
        emit themeToggled(); 
        // Icon update will happen via separate method or simple check
        if (btnTheme->text() == "☀") btnTheme->setText("🌙");
        else btnTheme->setText("☀");
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

void TopBar::updateTime()
{
    // 3초 느리게 표시 (User Request)
    QDateTime now = QDateTime::currentDateTime().addSecs(-3);
    dateLabel->setText(now.toString("yyyy-MM-dd"));
    timeLabel->setText(now.toString("HH:mm:ss"));
}

void TopBar::updateTheme(bool isDark)
{
    if (isDark) {
        // Dark Mode: Light Text
        titleLabel->setStyleSheet("font-weight: bold; font-size: 16px; letter-spacing: 1px; color: #e2e8f0; background-color: transparent;");
        if(btnToggle) btnToggle->setStyleSheet("QPushButton { color: #94a3b8; font-size: 20px; border: none; background: transparent; } QPushButton:hover { color: white; background: rgba(255,255,255,0.05); border-radius: 4px; }");
        
        // Date/Time Dark
        dateLabel->setStyleSheet("color: #64748b; font-family: monospace; font-size: 11px; font-weight: bold; background-color: transparent;");
        timeLabel->setStyleSheet("color: #135bec; font-family: monospace; font-weight: bold; font-size: 14px; background-color: transparent;");
    } else {
        // Light Mode: Dark Text (Gray 900)
        titleLabel->setStyleSheet("font-weight: bold; font-size: 16px; letter-spacing: 1px; color: #1e293b; background-color: transparent;");
        if(btnToggle) btnToggle->setStyleSheet("QPushButton { color: #475569; font-size: 20px; border: none; background: transparent; } QPushButton:hover { color: #1e293b; background: rgba(0,0,0,0.05); border-radius: 4px; }");
        
        // Date/Time Light
        dateLabel->setStyleSheet("color: #64748b; font-family: monospace; font-size: 11px; font-weight: bold; background-color: transparent;");
        timeLabel->setStyleSheet("color: #135bec; font-family: monospace; font-weight: bold; font-size: 14px; background-color: transparent;");
    }
}
