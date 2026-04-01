/**
 * @file topbar.cpp
 * @brief Top navigation bar implementation
 */
#include "topbar.h"
#include <QFontMetrics>
#include <QGuiApplication>
#include <QFont>
#include <QPainterPath>
#include <QStyle>
#include <QAbstractButton>
#include <QFrame>
#include <QListWidget>
#include <QMenu>
#include <QPixmap>
#include <QScreen>
#include <QVBoxLayout>

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

    auto *brandIconLabel = new QLabel(this);
    brandIconLabel->setObjectName("TopBarBrandIcon");
    brandIconLabel->setFixedHeight(40);
    brandIconLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    brandIconLabel->setStyleSheet(QStringLiteral("background: transparent;"));
    const QPixmap brandIcon(QStringLiteral(":/icons/assets/icons/noobigo_wordmark.png"));
    brandIconLabel->setPixmap(brandIcon.scaledToHeight(40, Qt::SmoothTransformation));

    titleLabel = new QLabel(QStringLiteral("누비고 프로그램"), this);
    titleLabel->setObjectName("TopBarTitle");
    QFont titleFont = titleLabel->font();
    titleFont.setFamilies({QStringLiteral("Arial Rounded MT Bold"),
                           QStringLiteral("휴먼둥근헤드라인"),
                           QStringLiteral("HYHeadLine M"),
                           QStringLiteral("Malgun Gothic"),
                           QStringLiteral("Segoe UI")});
    titleFont.setPointSize(15);
    titleFont.setWeight(QFont::DemiBold);
    titleLabel->setFont(titleFont);
    titleLabel->clear();
    titleLabel->setVisible(false);
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

    btnAlert = new QPushButton(QStringLiteral("⚠"), this);
    btnAlert->setFixedSize(34, 34);
    btnAlert->setCursor(Qt::PointingHandCursor);
    btnAlert->setObjectName("TopBarAlertButton");
    btnAlert->setToolTip(QStringLiteral("Fall alert log"));
    btnAlert->setProperty("hasUnread", false);

    btnClose = new QPushButton(QStringLiteral("×"), this);
    btnClose->setFixedSize(32, 32);
    btnClose->setCursor(Qt::PointingHandCursor);
    btnClose->setObjectName("TopBarCloseBtn");
    btnClose->setText(QStringLiteral("\u00D7"));
    btnClose->setStyleSheet(
        "QPushButton { color: #cbd5e1; font-size: 17px; font-weight: 500; border: none; "
        "background: transparent; border-radius: 8px; padding-bottom: 2px; }"
        "QPushButton:hover { color: #f8fafc; background: rgba(255, 255, 255, 0.08); }"
        "QPushButton:pressed { background: rgba(255, 255, 255, 0.14); }");

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
    layout->addWidget(brandIconLabel);
    layout->addWidget(titleLabel);
    layout->addStretch(); 
    layout->addWidget(navContainer);
    layout->addStretch();
    layout->addWidget(btnAlert);
    layout->addWidget(rightContainer);
    layout->addSpacing(10);
    layout->addWidget(btnTheme);
    layout->addWidget(userIcon);
    layout->addWidget(btnClose);

    // Connections
    connect(btnToggle, &QPushButton::clicked, this, &TopBar::sidebarToggled);
    connect(btnClose, &QPushButton::clicked, this, &TopBar::closeRequested);
    connect(btnAlert, &QPushButton::clicked, this, &TopBar::showFallAlertHistoryPopup);
    
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

void TopBar::appendFallAlertLog(const QString &entry)
{
    const QString trimmedEntry = entry.trimmed();
    if (trimmedEntry.isEmpty()) {
        return;
    }

    m_fallAlertLogs.append(trimmedEntry);
    while (m_fallAlertLogs.size() > 50) {
        m_fallAlertLogs.removeFirst();
    }

    m_hasUnreadFallAlerts = true;
    updateAlertButtonState();
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
    // Show the current local time.
    QDateTime now = QDateTime::currentDateTime();
    dateLabel->setText(now.toString("yyyy-MM-dd"));
    timeLabel->setText(now.toString("HH:mm:ss"));
}

void TopBar::showFallAlertHistoryPopup()
{
    if (!btnAlert) {
        return;
    }

    QWidget *popup = new QWidget(this);
    popup->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setAttribute(Qt::WA_StyledBackground, true);
    popup->setObjectName("TopBarAlertPopup");

    auto *popupLayout = new QVBoxLayout(popup);
    popupLayout->setContentsMargins(14, 14, 14, 14);
    popupLayout->setSpacing(10);

    auto *titleLabel = new QLabel(QStringLiteral("쓰러짐 감지 로그"), popup);
    titleLabel->setObjectName("TopBarAlertPopupTitle");
    popupLayout->addWidget(titleLabel);

    if (m_fallAlertLogs.isEmpty()) {
        auto *emptyLabel = new QLabel(QStringLiteral("쓰러짐 감지 로그 없음"), popup);
        emptyLabel->setObjectName("TopBarAlertPopupEmpty");
        emptyLabel->setWordWrap(true);
        popupLayout->addWidget(emptyLabel);
    } else {
        auto *listWidget = new QListWidget(popup);
        listWidget->setObjectName("TopBarAlertPopupList");
        listWidget->setSelectionMode(QAbstractItemView::NoSelection);
        listWidget->setFocusPolicy(Qt::NoFocus);
        listWidget->setFrameShape(QFrame::NoFrame);
        listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        listWidget->viewport()->setAutoFillBackground(false);

        for (int i = m_fallAlertLogs.size() - 1; i >= 0; --i) {
            listWidget->addItem(m_fallAlertLogs.at(i));
        }

        const int visibleRows = qMin(6, qMax(1, listWidget->count()));
        const int rowHeight = listWidget->sizeHintForRow(0) > 0 ? listWidget->sizeHintForRow(0) : QFontMetrics(listWidget->font()).lineSpacing() + 12;
        listWidget->setMinimumWidth(320);
        listWidget->setFixedHeight((rowHeight * visibleRows) + (listWidget->frameWidth() * 2) + 4);
        popupLayout->addWidget(listWidget);
    }

    popup->adjustSize();
    QPainterPath popupPath;
    popupPath.addRoundedRect(QRectF(popup->rect()), 14.0, 14.0);
    popup->setMask(QRegion(popupPath.toFillPolygon().toPolygon()));

    QPoint globalPos = btnAlert->mapToGlobal(QPoint(btnAlert->width() - popup->width(), btnAlert->height() + 8));
    if (QScreen *screen = QGuiApplication::screenAt(globalPos)) {
        const QRect available = screen->availableGeometry();
        if (globalPos.x() + popup->width() > available.right()) {
            globalPos.setX(available.right() - popup->width());
        }
        if (globalPos.x() < available.left()) {
            globalPos.setX(available.left());
        }
        if (globalPos.y() + popup->height() > available.bottom()) {
            globalPos.setY(btnAlert->mapToGlobal(QPoint(0, 0)).y() - popup->height() - 8);
        }
        if (globalPos.y() < available.top()) {
            globalPos.setY(available.top());
        }
    }

    popup->move(globalPos);
    popup->show();

    m_hasUnreadFallAlerts = false;
    updateAlertButtonState();
}

void TopBar::updateAlertButtonState()
{
    if (!btnAlert) {
        return;
    }

    btnAlert->setProperty("hasUnread", m_hasUnreadFallAlerts);
    btnAlert->setToolTip(m_fallAlertLogs.isEmpty()
                             ? QStringLiteral("Fall alert log")
                             : QStringLiteral("Fall alert log (%1)").arg(m_fallAlertLogs.size()));
    updateStyle(btnAlert);
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
