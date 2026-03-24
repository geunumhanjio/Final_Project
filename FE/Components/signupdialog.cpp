#include "signupdialog.h"

#include "authmanager.h"

#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QStyle>
#include <QVBoxLayout>
#include <QWindow>

namespace {

QString upperTag(QString value)
{
    return value.trimmed().toUpper();
}

bool looksLikeEmail(const QString &value)
{
    const QString trimmed = value.trimmed();
    return trimmed.contains(QLatin1Char('@')) && trimmed.contains(QLatin1Char('.'));
}

QSize boundedDialogSize(const QSize &contentSize)
{
    const QRect availableGeometry = QGuiApplication::primaryScreen()
        ? QGuiApplication::primaryScreen()->availableGeometry()
        : QRect(0, 0, 1280, 720);
    const QSize availableSize(qMax(420, availableGeometry.width() - 96),
                              qMax(480, availableGeometry.height() - 96));

    return QSize(qMin(contentSize.width(), availableSize.width()),
                 qMin(contentSize.height(), availableSize.height()));
}

} // namespace

SignUpDialog::SignUpDialog(const QString &baseUrl, bool darkTheme, QWidget *parent)
    : QDialog(parent)
    , m_baseUrl(baseUrl.trimmed())
    , m_isDark(darkTheme)
{
    setWindowTitle(QStringLiteral("Create Operator Account"));
    setModal(true);
    setObjectName("SignUpDialog");
    setAttribute(Qt::WA_StyledBackground, true);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 18);
    mainLayout->setSpacing(10);

    m_card = new QFrame(this);
    m_card->setObjectName("SignUpCard");
    m_card->setMaximumWidth(470);

    auto *cardLayout = new QVBoxLayout(m_card);
    cardLayout->setContentsMargins(24, 24, 24, 22);
    cardLayout->setSpacing(12);

    auto *badgeLabel = new QLabel(QStringLiteral("JOIN"), m_card);
    badgeLabel->setObjectName("SignUpBadge");
    badgeLabel->setAlignment(Qt::AlignCenter);
    badgeLabel->setFixedSize(58, 30);
    cardLayout->addWidget(badgeLabel, 0, Qt::AlignLeft);

    m_titleLabel = new QLabel(m_card);
    m_titleLabel->setObjectName("SignUpTitle");
    cardLayout->addWidget(m_titleLabel);

    m_subtitleLabel = new QLabel(m_card);
    m_subtitleLabel->setObjectName("SignUpSubtitle");
    m_subtitleLabel->setWordWrap(true);
    cardLayout->addWidget(m_subtitleLabel);

    m_gatewayLabel = new QLabel(m_card);
    m_gatewayLabel->setObjectName("SignUpGateway");
    m_gatewayLabel->setWordWrap(true);
    m_gatewayLabel->setText(QStringLiteral("Gateway: %1").arg(m_baseUrl));
    cardLayout->addWidget(m_gatewayLabel);

    auto *userIdLabel = new QLabel(QStringLiteral("Operator Identity"), m_card);
    auto *nameLabel = new QLabel(QStringLiteral("Display Name"), m_card);
    auto *emailLabel = new QLabel(QStringLiteral("Email Address"), m_card);
    auto *passwordLabel = new QLabel(QStringLiteral("Access Key"), m_card);
    auto *confirmLabel = new QLabel(QStringLiteral("Confirm Key"), m_card);

    userIdLabel->setProperty("signupRole", "fieldLabel");
    nameLabel->setProperty("signupRole", "fieldLabel");
    emailLabel->setProperty("signupRole", "fieldLabel");
    passwordLabel->setProperty("signupRole", "fieldLabel");
    confirmLabel->setProperty("signupRole", "fieldLabel");

    m_userIdEdit = new QLineEdit(m_card);
    m_userIdEdit->setPlaceholderText(QStringLiteral("admin"));

    m_nameEdit = new QLineEdit(m_card);
    m_nameEdit->setPlaceholderText(QStringLiteral("Administrator"));

    m_emailEdit = new QLineEdit(m_card);
    m_emailEdit->setPlaceholderText(QStringLiteral("admin@example.com"));

    m_passwordEdit = new QLineEdit(m_card);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(QStringLiteral("Create a password"));

    m_confirmPasswordEdit = new QLineEdit(m_card);
    m_confirmPasswordEdit->setEchoMode(QLineEdit::Password);
    m_confirmPasswordEdit->setPlaceholderText(QStringLiteral("Re-enter the password"));

    m_passwordToggleButton = new QPushButton(QStringLiteral("SHOW"), m_card);
    m_passwordToggleButton->setObjectName("SignUpPasswordToggle");
    m_passwordToggleButton->setCursor(Qt::PointingHandCursor);
    m_passwordToggleButton->setFixedHeight(30);
    connect(m_passwordToggleButton, &QPushButton::clicked, this, [this]() {
        const bool showPlainText = (m_passwordEdit->echoMode() == QLineEdit::Password);
        m_passwordEdit->setEchoMode(showPlainText ? QLineEdit::Normal : QLineEdit::Password);
        m_passwordToggleButton->setText(showPlainText ? QStringLiteral("HIDE") : QStringLiteral("SHOW"));
    });

    m_confirmPasswordToggleButton = new QPushButton(QStringLiteral("SHOW"), m_card);
    m_confirmPasswordToggleButton->setObjectName("SignUpPasswordToggle");
    m_confirmPasswordToggleButton->setCursor(Qt::PointingHandCursor);
    m_confirmPasswordToggleButton->setFixedHeight(30);
    connect(m_confirmPasswordToggleButton, &QPushButton::clicked, this, [this]() {
        const bool showPlainText = (m_confirmPasswordEdit->echoMode() == QLineEdit::Password);
        m_confirmPasswordEdit->setEchoMode(showPlainText ? QLineEdit::Normal : QLineEdit::Password);
        m_confirmPasswordToggleButton->setText(showPlainText ? QStringLiteral("HIDE") : QStringLiteral("SHOW"));
    });

    cardLayout->addWidget(userIdLabel);
    cardLayout->addWidget(createFieldShell(QStringLiteral("ID"), m_userIdEdit));
    cardLayout->addWidget(nameLabel);
    cardLayout->addWidget(createFieldShell(QStringLiteral("NAME"), m_nameEdit));
    cardLayout->addWidget(emailLabel);
    cardLayout->addWidget(createFieldShell(QStringLiteral("MAIL"), m_emailEdit));
    cardLayout->addWidget(passwordLabel);
    cardLayout->addWidget(createFieldShell(QStringLiteral("KEY"), m_passwordEdit, m_passwordToggleButton));
    cardLayout->addWidget(confirmLabel);
    cardLayout->addWidget(createFieldShell(QStringLiteral("CONF"), m_confirmPasswordEdit, m_confirmPasswordToggleButton));

    m_statusLabel = new QLabel(m_card);
    m_statusLabel->setObjectName("SignUpStatus");
    m_statusLabel->setWordWrap(true);
    cardLayout->addWidget(m_statusLabel);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 4, 0, 0);
    buttonLayout->setSpacing(10);

    m_cancelButton = new QPushButton(QStringLiteral("Cancel"), m_card);
    m_cancelButton->setObjectName("SignUpSecondaryButton");
    m_cancelButton->setCursor(Qt::PointingHandCursor);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    m_submitButton = new QPushButton(m_card);
    m_submitButton->setObjectName("SignUpPrimaryButton");
    m_submitButton->setCursor(Qt::PointingHandCursor);
    m_submitButton->setMinimumHeight(48);
    connect(m_submitButton, &QPushButton::clicked, this, &SignUpDialog::attemptRegistration);

    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_submitButton, 1);
    cardLayout->addLayout(buttonLayout);

    mainLayout->addWidget(m_card, 0, Qt::AlignCenter);

    connect(m_userIdEdit, &QLineEdit::returnPressed, this, &SignUpDialog::attemptRegistration);
    connect(m_nameEdit, &QLineEdit::returnPressed, this, &SignUpDialog::attemptRegistration);
    connect(m_emailEdit, &QLineEdit::returnPressed, this, &SignUpDialog::attemptRegistration);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &SignUpDialog::attemptRegistration);
    connect(m_confirmPasswordEdit, &QLineEdit::returnPressed, this, &SignUpDialog::attemptRegistration);
    connect(&AuthManager::instance(), &AuthManager::registrationSucceeded, this, &SignUpDialog::handleRegistrationSucceeded);
    connect(&AuthManager::instance(), &AuthManager::registrationFailed, this, &SignUpDialog::handleRegistrationFailed);

    applyTheme();
    updateStatusMessage(QStringLiteral("Create a new operator profile. The client will send id, name, email, and password to POST /users."));

    layout()->activate();
    adjustSize();
    const QSize dialogSize = boundedDialogSize(sizeHint().expandedTo(QSize(520, 650)));
    setFixedSize(dialogSize);

    m_userIdEdit->setFocus();
}

void SignUpDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_isDark) {
        QLinearGradient background(0, 0, width(), height());
        background.setColorAt(0.0, QColor("#07101c"));
        background.setColorAt(0.5, QColor("#0b1326"));
        background.setColorAt(1.0, QColor("#12274b"));
        painter.fillRect(rect(), background);

        painter.setPen(QPen(QColor(173, 198, 255, 34), 1.0));
        painter.drawLine(28, 28, 148, 28);
        painter.drawLine(28, 28, 28, 148);
        painter.drawLine(width() - 28, height() - 28, width() - 148, height() - 28);
        painter.drawLine(width() - 28, height() - 28, width() - 28, height() - 148);
    } else {
        QLinearGradient background(0, 0, width(), height());
        background.setColorAt(0.0, QColor("#fffdf8"));
        background.setColorAt(1.0, QColor("#eef6ff"));
        painter.fillRect(rect(), background);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(196, 198, 209, 72));
        for (int y = 20; y < height(); y += 36) {
            for (int x = 20; x < width(); x += 36) {
                painter.drawEllipse(QPointF(x, y), 1.1, 1.1);
            }
        }
    }
}

void SignUpDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    positionDialog();
}

void SignUpDialog::attemptRegistration()
{
    if (m_busy) {
        return;
    }

    const QString userId = m_userIdEdit->text().trimmed();
    const QString name = m_nameEdit->text().trimmed();
    const QString email = m_emailEdit->text().trimmed();
    const QString password = m_passwordEdit->text();
    const QString confirmPassword = m_confirmPasswordEdit->text();

    if (m_baseUrl.isEmpty()) {
        updateStatusMessage(QStringLiteral("Login server URL is missing. Return to the login screen and verify the gateway address."), true);
        return;
    }

    if (userId.isEmpty()) {
        updateStatusMessage(QStringLiteral("Operator ID is required."), true);
        m_userIdEdit->setFocus();
        return;
    }

    if (name.isEmpty()) {
        updateStatusMessage(QStringLiteral("Display name is required."), true);
        m_nameEdit->setFocus();
        return;
    }

    if (!looksLikeEmail(email)) {
        updateStatusMessage(QStringLiteral("Enter a valid email address."), true);
        m_emailEdit->setFocus();
        return;
    }

    if (password.isEmpty()) {
        updateStatusMessage(QStringLiteral("Password is required."), true);
        m_passwordEdit->setFocus();
        return;
    }

    if (password != confirmPassword) {
        updateStatusMessage(QStringLiteral("The password confirmation does not match."), true);
        m_confirmPasswordEdit->selectAll();
        m_confirmPasswordEdit->setFocus();
        return;
    }

    setBusy(true);
    updateStatusMessage(QStringLiteral("Submitting operator enrollment request to POST /users..."));
    AuthManager::instance().registerUser(m_baseUrl, userId, name, email, password);
}

void SignUpDialog::handleRegistrationSucceeded(const QString &userId)
{
    m_registeredUserId = userId.trimmed();
    setBusy(false);
    updateStatusMessage(QStringLiteral("Operator account created successfully. Return to login with the new credentials."));
    accept();
}

void SignUpDialog::handleRegistrationFailed(const QString &message)
{
    setBusy(false);
    updateStatusMessage(message.isEmpty() ? QStringLiteral("Registration failed.") : message, true);
}

QWidget *SignUpDialog::createFieldShell(const QString &tag, QLineEdit *edit, QWidget *trailingWidget)
{
    auto *shell = new QFrame(m_card);
    shell->setObjectName("SignUpFieldShell");

    auto *layout = new QHBoxLayout(shell);
    layout->setContentsMargins(14, 8, 12, 8);
    layout->setSpacing(10);

    auto *tagLabel = new QLabel(upperTag(tag), shell);
    tagLabel->setProperty("signupRole", "fieldTag");
    tagLabel->setAlignment(Qt::AlignCenter);
    tagLabel->setMinimumWidth(40);

    edit->setProperty("signupRole", "fieldEdit");
    edit->setFrame(false);
    edit->setMinimumHeight(36);

    layout->addWidget(tagLabel);
    layout->addWidget(edit, 1);

    if (trailingWidget) {
        layout->addWidget(trailingWidget, 0, Qt::AlignVCenter);
    }

    return shell;
}

void SignUpDialog::applyTheme()
{
    m_titleLabel->setText(m_isDark
                              ? QStringLiteral("Create Secure Operator")
                              : QStringLiteral("Register Operator"));
    m_subtitleLabel->setText(m_isDark
                                 ? QStringLiteral("Provision a user record on the JWT login server before attempting RTSPS access.")
                                 : QStringLiteral("Send a valid operator profile to the login server and then sign in from the main access screen."));
    m_submitButton->setText(m_busy ? QStringLiteral("CREATING ACCOUNT...") : QStringLiteral("CREATE ACCOUNT"));

    const QString styleSheet = m_isDark
        ? QStringLiteral(
            "QDialog#SignUpDialog {"
            " color: #dae2fd;"
            "}"
            "#SignUpCard {"
            " background: rgba(11, 19, 38, 0.92);"
            " border: 1px solid rgba(69, 70, 77, 0.45);"
            " border-radius: 24px;"
            "}"
            "#SignUpBadge {"
            " background: rgba(23, 31, 51, 0.96);"
            " color: #adc6ff;"
            " border: 1px solid rgba(173, 198, 255, 0.22);"
            " border-radius: 15px;"
            " font-size: 11px;"
            " font-weight: 800;"
            " letter-spacing: 1px;"
            "}"
            "#SignUpTitle {"
            " color: #f8fbff;"
            " font-size: 24px;"
            " font-weight: 800;"
            " background: transparent;"
            "}"
            "#SignUpSubtitle {"
            " color: rgba(198, 198, 205, 0.82);"
            " font-size: 13px;"
            " line-height: 1.5;"
            " background: transparent;"
            "}"
            "#SignUpGateway {"
            " background: rgba(23, 31, 51, 0.92);"
            " color: #adc6ff;"
            " border: 1px solid rgba(173, 198, 255, 0.12);"
            " border-radius: 14px;"
            " padding: 10px 12px;"
            " font-size: 12px;"
            " font-weight: 600;"
            "}"
            "QLabel[signupRole=\"fieldLabel\"] {"
            " color: #c6c6cd;"
            " font-size: 10px;"
            " font-weight: 800;"
            " letter-spacing: 2px;"
            " text-transform: uppercase;"
            " background: transparent;"
            " margin-left: 4px;"
            "}"
            "#SignUpFieldShell {"
            " background: rgba(6, 14, 32, 0.95);"
            " border: 1px solid rgba(69, 70, 77, 0.65);"
            " border-radius: 16px;"
            "}"
            "#SignUpFieldShell:focus-within {"
            " border: 1px solid #adc6ff;"
            "}"
            "QLabel[signupRole=\"fieldTag\"] {"
            " color: #909097;"
            " font-size: 11px;"
            " font-weight: 800;"
            " letter-spacing: 1px;"
            " background: transparent;"
            "}"
            "QLineEdit[signupRole=\"fieldEdit\"] {"
            " background: transparent;"
            " color: #edf2ff;"
            " border: none;"
            " font-size: 14px;"
            " font-weight: 600;"
            " selection-background-color: #357df1;"
            "}"
            "QLineEdit[signupRole=\"fieldEdit\"]::placeholder {"
            " color: rgba(144, 144, 151, 0.65);"
            "}"
            "#SignUpPasswordToggle {"
            " background: rgba(45, 52, 73, 0.85);"
            " color: #c6d8ff;"
            " border: 1px solid rgba(173, 198, 255, 0.18);"
            " border-radius: 12px;"
            " padding: 0 14px;"
            " font-size: 11px;"
            " font-weight: 800;"
            " letter-spacing: 1px;"
            "}"
            "#SignUpPasswordToggle:hover {"
            " background: rgba(53, 125, 241, 0.18);"
            "}"
            "#SignUpSecondaryButton {"
            " background: rgba(23, 31, 51, 0.88);"
            " color: #c6c6cd;"
            " border: 1px solid rgba(69, 70, 77, 0.65);"
            " border-radius: 14px;"
            " font-size: 13px;"
            " font-weight: 700;"
            " padding: 0 18px;"
            "}"
            "#SignUpSecondaryButton:hover {"
            " background: rgba(45, 52, 73, 0.95);"
            "}"
            "#SignUpPrimaryButton {"
            " background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #adc6ff, stop:1 #357df1);"
            " color: #002e6a;"
            " border: none;"
            " border-radius: 16px;"
            " font-size: 15px;"
            " font-weight: 800;"
            " letter-spacing: 0.6px;"
            "}"
            "#SignUpPrimaryButton:hover {"
            " background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #c4d7ff, stop:1 #4d8df3);"
            "}"
            "#SignUpPrimaryButton:disabled, #SignUpSecondaryButton:disabled {"
            " background: #41506d;"
            " color: rgba(237, 242, 255, 0.65);"
            "}"
            "#SignUpStatus {"
            " color: #cbd5e1;"
            " font-size: 12px;"
            " background: transparent;"
            "}"
        )
        : QStringLiteral(
            "QDialog#SignUpDialog {"
            " color: #283044;"
            "}"
            "#SignUpCard {"
            " background: rgba(255, 255, 255, 0.96);"
            " border: 1px solid rgba(196, 198, 209, 0.70);"
            " border-radius: 24px;"
            "}"
            "#SignUpBadge {"
            " background: rgba(255, 102, 0, 0.10);"
            " color: #ff6600;"
            " border: 1px solid rgba(255, 102, 0, 0.20);"
            " border-radius: 15px;"
            " font-size: 11px;"
            " font-weight: 900;"
            " letter-spacing: 1px;"
            "}"
            "#SignUpTitle {"
            " color: #191c20;"
            " font-size: 26px;"
            " font-weight: 900;"
            " background: transparent;"
            "}"
            "#SignUpSubtitle {"
            " color: rgba(68, 70, 80, 0.88);"
            " font-size: 13px;"
            " line-height: 1.5;"
            " background: transparent;"
            "}"
            "#SignUpGateway {"
            " background: #f6f8fc;"
            " color: #444650;"
            " border: 1px solid rgba(196, 198, 209, 0.70);"
            " border-radius: 14px;"
            " padding: 10px 12px;"
            " font-size: 12px;"
            " font-weight: 700;"
            "}"
            "QLabel[signupRole=\"fieldLabel\"] {"
            " color: #5c5e6a;"
            " font-size: 10px;"
            " font-weight: 800;"
            " letter-spacing: 2px;"
            " text-transform: uppercase;"
            " background: transparent;"
            " margin-left: 4px;"
            "}"
            "#SignUpFieldShell {"
            " background: #f6f8fc;"
            " border: 1px solid rgba(196, 198, 209, 0.85);"
            " border-radius: 16px;"
            "}"
            "#SignUpFieldShell:focus-within {"
            " border: 1px solid #ff6600;"
            "}"
            "QLabel[signupRole=\"fieldTag\"] {"
            " color: #74777f;"
            " font-size: 11px;"
            " font-weight: 800;"
            " letter-spacing: 1px;"
            " background: transparent;"
            "}"
            "QLineEdit[signupRole=\"fieldEdit\"] {"
            " background: transparent;"
            " color: #191c20;"
            " border: none;"
            " font-size: 14px;"
            " font-weight: 700;"
            " selection-background-color: #ffb37d;"
            "}"
            "QLineEdit[signupRole=\"fieldEdit\"]::placeholder {"
            " color: rgba(116, 119, 127, 0.68);"
            "}"
            "#SignUpPasswordToggle {"
            " background: rgba(255, 102, 0, 0.08);"
            " color: #ff6600;"
            " border: 1px solid rgba(255, 102, 0, 0.18);"
            " border-radius: 12px;"
            " padding: 0 14px;"
            " font-size: 11px;"
            " font-weight: 800;"
            " letter-spacing: 1px;"
            "}"
            "#SignUpPasswordToggle:hover {"
            " background: rgba(255, 102, 0, 0.16);"
            "}"
            "#SignUpSecondaryButton {"
            " background: #ffffff;"
            " color: #444650;"
            " border: 1px solid rgba(196, 198, 209, 0.95);"
            " border-radius: 14px;"
            " font-size: 13px;"
            " font-weight: 800;"
            " padding: 0 18px;"
            "}"
            "#SignUpSecondaryButton:hover {"
            " border-color: rgba(255, 102, 0, 0.40);"
            " color: #ff6600;"
            "}"
            "#SignUpPrimaryButton {"
            " background: #ff6600;"
            " color: white;"
            " border: none;"
            " border-radius: 16px;"
            " font-size: 15px;"
            " font-weight: 900;"
            " letter-spacing: 0.6px;"
            "}"
            "#SignUpPrimaryButton:hover {"
            " background: #e65c00;"
            "}"
            "#SignUpPrimaryButton:disabled, #SignUpSecondaryButton:disabled {"
            " background: #d2d8e4;"
            " color: rgba(25, 28, 32, 0.55);"
            "}"
            "#SignUpStatus {"
            " color: #5c5e6a;"
            " font-size: 12px;"
            " background: transparent;"
            "}"
        );

    setStyleSheet(styleSheet);
    updateStatusMessage(m_statusLabel->text(), m_statusLabel->property("statusError").toBool());
}

void SignUpDialog::setBusy(bool busy)
{
    m_busy = busy;
    m_userIdEdit->setEnabled(!busy);
    m_nameEdit->setEnabled(!busy);
    m_emailEdit->setEnabled(!busy);
    m_passwordEdit->setEnabled(!busy);
    m_confirmPasswordEdit->setEnabled(!busy);
    m_passwordToggleButton->setEnabled(!busy);
    m_confirmPasswordToggleButton->setEnabled(!busy);
    m_cancelButton->setEnabled(!busy);
    m_submitButton->setEnabled(!busy);
    applyTheme();
}

void SignUpDialog::positionDialog()
{
    QRect referenceGeometry;

    if (QWidget *parent = parentWidget()) {
        referenceGeometry = QRect(parent->mapToGlobal(QPoint(0, 0)), parent->size());
    } else if (windowHandle() && windowHandle()->screen()) {
        referenceGeometry = windowHandle()->screen()->availableGeometry();
    } else if (QScreen *screen = QGuiApplication::primaryScreen()) {
        referenceGeometry = screen->availableGeometry();
    }

    if (!referenceGeometry.isValid()) {
        return;
    }

    const QRect alignedRect = QStyle::alignedRect(layoutDirection(), Qt::AlignCenter, size(), referenceGeometry);
    move(alignedRect.topLeft());
}

void SignUpDialog::updateStatusMessage(const QString &message, bool isError)
{
    m_statusLabel->setProperty("statusError", isError);
    m_statusLabel->setText(message);

    const QString color = isError
        ? (m_isDark ? QStringLiteral("#ffb4ab") : QStringLiteral("#ba1a1a"))
        : (m_isDark ? QStringLiteral("#cbd5e1") : QStringLiteral("#5c5e6a"));

    m_statusLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; background: transparent;").arg(color));
}
