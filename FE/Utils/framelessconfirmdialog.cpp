#include "framelessconfirmdialog.h"

#include <QColor>
#include <QDialog>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace {

struct DialogVisualConfig
{
    QString panelObjectName;
    QString titleObjectName;
    QString textObjectName;
    QString iconObjectName;
    QString cancelObjectName;
    QString acceptObjectName;
    QString iconText;
    QString title;
    QString message;
    QString cancelText;
    QString acceptText;
    int minWidth = 360;
    QString darkStyle;
    QString lightStyle;
};

bool showDialog(QWidget *parent, bool darkTheme, const DialogVisualConfig &config)
{
    QDialog dialog(parent, Qt::Dialog | Qt::FramelessWindowHint | Qt::CustomizeWindowHint);
    dialog.setModal(true);
    dialog.setAttribute(Qt::WA_TranslucentBackground);

    auto *rootLayout = new QVBoxLayout(&dialog);
    rootLayout->setContentsMargins(20, 20, 20, 20);

    auto *panel = new QFrame(&dialog);
    panel->setObjectName(config.panelObjectName);
    panel->setMinimumWidth(config.minWidth);
    panel->setStyleSheet(darkTheme ? config.darkStyle : config.lightStyle);

    auto *shadow = new QGraphicsDropShadowEffect(panel);
    shadow->setBlurRadius(36);
    shadow->setOffset(0, 14);
    shadow->setColor(darkTheme ? QColor(0, 0, 0, 150) : QColor(15, 23, 42, 45));
    panel->setGraphicsEffect(shadow);

    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(24, 22, 24, 20);
    panelLayout->setSpacing(18);

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(14);

    auto *iconLabel = new QLabel(config.iconText, panel);
    iconLabel->setObjectName(config.iconObjectName);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setFixedSize(44, 44);

    auto *titleWrap = new QVBoxLayout();
    titleWrap->setSpacing(4);
    titleWrap->setContentsMargins(0, 0, 0, 0);

    auto *titleLabel = new QLabel(config.title, panel);
    titleLabel->setObjectName(config.titleObjectName);

    auto *messageLabel = new QLabel(config.message, panel);
    messageLabel->setObjectName(config.textObjectName);
    messageLabel->setWordWrap(true);

    titleWrap->addWidget(titleLabel);
    titleWrap->addWidget(messageLabel);

    headerLayout->addWidget(iconLabel, 0, Qt::AlignTop);
    headerLayout->addLayout(titleWrap, 1);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    buttonLayout->addStretch();

    auto *cancelButton = new QPushButton(config.cancelText, panel);
    cancelButton->setObjectName(config.cancelObjectName);
    cancelButton->setCursor(Qt::PointingHandCursor);

    auto *acceptButton = new QPushButton(config.acceptText, panel);
    acceptButton->setObjectName(config.acceptObjectName);
    acceptButton->setCursor(Qt::PointingHandCursor);

    QObject::connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(acceptButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(acceptButton);

    panelLayout->addLayout(headerLayout);
    panelLayout->addLayout(buttonLayout);
    rootLayout->addWidget(panel);

    dialog.adjustSize();
    if (parent) {
        dialog.move(parent->frameGeometry().center() - dialog.rect().center());
    }

    return dialog.exec() == QDialog::Accepted;
}

} // namespace

bool showCloseConfirmationDialog(QWidget *parent, bool darkTheme)
{
    return showDialog(parent, darkTheme, {
        QStringLiteral("CloseConfirmPanel"),
        QStringLiteral("CloseConfirmTitle"),
        QStringLiteral("CloseConfirmText"),
        QStringLiteral("CloseConfirmIcon"),
        QStringLiteral("CloseConfirmCancel"),
        QStringLiteral("CloseConfirmAccept"),
        QStringLiteral("X"),
        QStringLiteral("앱 종료"),
        QStringLiteral("종료하시겠습니까?"),
        QStringLiteral("아니요"),
        QStringLiteral("예"),
        360,
        QStringLiteral(
            "QFrame#CloseConfirmPanel {"
            " background-color: #182131;"
            " border: 1px solid #2a3649;"
            " border-radius: 18px;"
            "}"
            "QLabel#CloseConfirmTitle {"
            " color: #f8fafc;"
            " font-size: 20px;"
            " font-weight: 700;"
            " background: transparent;"
            "}"
            "QLabel#CloseConfirmText {"
            " color: #cbd5e1;"
            " font-size: 14px;"
            " background: transparent;"
            "}"
            "QLabel#CloseConfirmIcon {"
            " background: rgba(239, 68, 68, 0.16);"
            " color: #f87171;"
            " border: 1px solid rgba(248, 113, 113, 0.28);"
            " border-radius: 22px;"
            " font-size: 18px;"
            " font-weight: 700;"
            "}"
            "QPushButton#CloseConfirmCancel {"
            " background-color: #243041;"
            " color: #e2e8f0;"
            " border: 1px solid #334155;"
            " border-radius: 10px;"
            " padding: 10px 18px;"
            " min-width: 88px;"
            "}"
            "QPushButton#CloseConfirmCancel:hover { background-color: #2d3b4f; }"
            "QPushButton#CloseConfirmAccept {"
            " background-color: #dc2626;"
            " color: white;"
            " border: none;"
            " border-radius: 10px;"
            " padding: 10px 18px;"
            " min-width: 88px;"
            "}"
            "QPushButton#CloseConfirmAccept:hover { background-color: #b91c1c; }"),
        QStringLiteral(
            "QFrame#CloseConfirmPanel {"
            " background-color: #ffffff;"
            " border: 1px solid #e2e8f0;"
            " border-radius: 18px;"
            "}"
            "QLabel#CloseConfirmTitle {"
            " color: #0f172a;"
            " font-size: 20px;"
            " font-weight: 700;"
            " background: transparent;"
            "}"
            "QLabel#CloseConfirmText {"
            " color: #475569;"
            " font-size: 14px;"
            " background: transparent;"
            "}"
            "QLabel#CloseConfirmIcon {"
            " background: #fff1f2;"
            " color: #dc2626;"
            " border: 1px solid #fecdd3;"
            " border-radius: 22px;"
            " font-size: 18px;"
            " font-weight: 700;"
            "}"
            "QPushButton#CloseConfirmCancel {"
            " background-color: #f8fafc;"
            " color: #0f172a;"
            " border: 1px solid #cbd5e1;"
            " border-radius: 10px;"
            " padding: 10px 18px;"
            " min-width: 88px;"
            "}"
            "QPushButton#CloseConfirmCancel:hover { background-color: #f1f5f9; }"
            "QPushButton#CloseConfirmAccept {"
            " background-color: #ef4444;"
            " color: white;"
            " border: none;"
            " border-radius: 10px;"
            " padding: 10px 18px;"
            " min-width: 88px;"
            "}"
            "QPushButton#CloseConfirmAccept:hover { background-color: #dc2626; }")
    });
}

bool showLogoutConfirmationDialog(QWidget *parent, bool darkTheme)
{
    return showDialog(parent, darkTheme, {
        QStringLiteral("LogoutConfirmPanel"),
        QStringLiteral("LogoutConfirmTitle"),
        QStringLiteral("LogoutConfirmText"),
        QStringLiteral("LogoutConfirmIcon"),
        QStringLiteral("LogoutConfirmCancel"),
        QStringLiteral("LogoutConfirmAccept"),
        QStringLiteral("->"),
        QStringLiteral("Log Out"),
        QStringLiteral("Do you want to sign out and return to the login screen?"),
        QStringLiteral("No"),
        QStringLiteral("Yes"),
        380,
        QStringLiteral(
            "QFrame#LogoutConfirmPanel {"
            " background-color: #182131;"
            " border: 1px solid #2a3649;"
            " border-radius: 18px;"
            "}"
            "QLabel#LogoutConfirmTitle {"
            " color: #f8fafc;"
            " font-size: 20px;"
            " font-weight: 700;"
            " background: transparent;"
            "}"
            "QLabel#LogoutConfirmText {"
            " color: #cbd5e1;"
            " font-size: 14px;"
            " background: transparent;"
            "}"
            "QLabel#LogoutConfirmIcon {"
            " background: rgba(59, 130, 246, 0.16);"
            " color: #93c5fd;"
            " border: 1px solid rgba(147, 197, 253, 0.28);"
            " border-radius: 22px;"
            " font-size: 18px;"
            " font-weight: 700;"
            "}"
            "QPushButton#LogoutConfirmCancel {"
            " background-color: #243041;"
            " color: #e2e8f0;"
            " border: 1px solid #334155;"
            " border-radius: 10px;"
            " padding: 10px 18px;"
            " min-width: 88px;"
            "}"
            "QPushButton#LogoutConfirmCancel:hover { background-color: #2d3b4f; }"
            "QPushButton#LogoutConfirmAccept {"
            " background-color: #2563eb;"
            " color: white;"
            " border: none;"
            " border-radius: 10px;"
            " padding: 10px 18px;"
            " min-width: 88px;"
            "}"
            "QPushButton#LogoutConfirmAccept:hover { background-color: #1d4ed8; }"),
        QStringLiteral(
            "QFrame#LogoutConfirmPanel {"
            " background-color: #ffffff;"
            " border: 1px solid #e2e8f0;"
            " border-radius: 18px;"
            "}"
            "QLabel#LogoutConfirmTitle {"
            " color: #0f172a;"
            " font-size: 20px;"
            " font-weight: 700;"
            " background: transparent;"
            "}"
            "QLabel#LogoutConfirmText {"
            " color: #475569;"
            " font-size: 14px;"
            " background: transparent;"
            "}"
            "QLabel#LogoutConfirmIcon {"
            " background: #eff6ff;"
            " color: #2563eb;"
            " border: 1px solid #bfdbfe;"
            " border-radius: 22px;"
            " font-size: 18px;"
            " font-weight: 700;"
            "}"
            "QPushButton#LogoutConfirmCancel {"
            " background-color: #f8fafc;"
            " color: #0f172a;"
            " border: 1px solid #cbd5e1;"
            " border-radius: 10px;"
            " padding: 10px 18px;"
            " min-width: 88px;"
            "}"
            "QPushButton#LogoutConfirmCancel:hover { background-color: #f1f5f9; }"
            "QPushButton#LogoutConfirmAccept {"
            " background-color: #2563eb;"
            " color: white;"
            " border: none;"
            " border-radius: 10px;"
            " padding: 10px 18px;"
            " min-width: 88px;"
            "}"
            "QPushButton#LogoutConfirmAccept:hover { background-color: #1d4ed8; }")
    });
}
