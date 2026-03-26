#include "logindialog.h"

#include "authmanager.h"
#include "configmanager.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QUrl>
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

bool isLocalMasterLogin(const QString &userId, const QString &password)
{
    return userId == QStringLiteral("admin") && password == QStringLiteral("admin");
}

QPoint globalMousePosition(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
}

QSize boundedDialogSize(const QSize &contentSize)
{
    const QRect availableGeometry = QGuiApplication::primaryScreen()
        ? QGuiApplication::primaryScreen()->availableGeometry()
        : QRect(0, 0, 1280, 720);
    const QSize availableSize(qMax(440, availableGeometry.width() - 96),
                              qMax(480, availableGeometry.height() - 96));

    return QSize(qMin(contentSize.width(), availableSize.width()),
                 qMin(contentSize.height(), availableSize.height()));
}

QString extractServerHost(QString value)
{
    value = value.trimmed();
    if (value.isEmpty()) {
        return QString();
    }

    QUrl url(value);
    if (url.host().isEmpty() && !value.contains(QStringLiteral("://"))) {
        url = QUrl(QStringLiteral("http://") + value);
    }

    QString host = url.host().trimmed();
    if (!host.isEmpty()) {
        return host;
    }

    QString fallback = value;
    const int schemeIndex = fallback.indexOf(QStringLiteral("://"));
    if (schemeIndex >= 0) {
        fallback = fallback.mid(schemeIndex + 3);
    }
    if (fallback.startsWith(QStringLiteral("//"))) {
        fallback.remove(0, 2);
    }

    const int slashIndex = fallback.indexOf(QLatin1Char('/'));
    if (slashIndex >= 0) {
        fallback = fallback.left(slashIndex);
    }

    const int colonIndex = fallback.indexOf(QLatin1Char(':'));
    if (colonIndex >= 0) {
        fallback = fallback.left(colonIndex);
    }

    return fallback.trimmed();
}

} // namespace

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    ConfigManager::instance().loadDefaults();
    m_isDark = ConfigManager::instance().getDarkTheme();

    setWindowTitle(QStringLiteral("누비고 로그인"));
    setModal(true);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::CustomizeWindowHint);
    setObjectName("LoginDialog");
    setAttribute(Qt::WA_StyledBackground, true);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 14, 16, 16);
    mainLayout->setSpacing(10);

    auto *topBarLayout = new QHBoxLayout();
    topBarLayout->setContentsMargins(0, 0, 0, 0);
    topBarLayout->setSpacing(8);
    topBarLayout->addStretch();

    m_themeButton = new QPushButton(this);
    m_themeButton->setObjectName("LoginTopButton");
    m_themeButton->setCursor(Qt::PointingHandCursor);
    m_themeButton->setFixedHeight(34);
    connect(m_themeButton, &QPushButton::clicked, this, &LoginDialog::toggleTheme);
    topBarLayout->addWidget(m_themeButton);

    m_closeButton = new QPushButton(QStringLiteral("CLOSE"), this);
    m_closeButton->setObjectName("LoginCloseButton");
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setFixedHeight(34);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);
    topBarLayout->addWidget(m_closeButton);

    mainLayout->addLayout(topBarLayout);

    m_root = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(m_root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(8);

    m_card = new QFrame(m_root);
    m_card->setObjectName("LoginCard");
    m_card->setFixedWidth(432);

    auto *cardLayout = new QVBoxLayout(m_card);
    cardLayout->setContentsMargins(22, 22, 22, 20);
    cardLayout->setSpacing(10);

    m_cardAccent = new QWidget(m_card);
    m_cardAccent->setObjectName("LoginCardAccent");
    m_cardAccent->setGeometry(QRect(0, 0, 44, 44));
    m_cardAccent->raise();

    m_cardTitleLabel = new QLabel(m_card);
    m_cardTitleLabel->setObjectName("LoginCardTitle");

    m_cardSubtitleLabel = new QLabel(m_card);
    m_cardSubtitleLabel->setObjectName("LoginCardSubtitle");
    m_cardSubtitleLabel->setWordWrap(true);
    m_cardSubtitleLabel->setVisible(false);

    auto *titleRow = new QWidget(m_card);
    titleRow->setObjectName("LoginTitleRow");
    auto *titleRowLayout = new QHBoxLayout(titleRow);
    titleRowLayout->setContentsMargins(24, 0, 0, 0);
    titleRowLayout->setSpacing(10);

    auto *brandIconLabel = new QLabel(titleRow);
    brandIconLabel->setObjectName("LoginBrandIcon");
    brandIconLabel->setFixedSize(42, 42);
    brandIconLabel->setAlignment(Qt::AlignCenter);
    const QPixmap brandIcon(QStringLiteral(":/icons/assets/icons/nubigo_robot.svg"));
    brandIconLabel->setPixmap(brandIcon.scaled(38, 38, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    titleRowLayout->addWidget(brandIconLabel, 0, Qt::AlignVCenter);
    titleRowLayout->addWidget(m_cardTitleLabel, 1, Qt::AlignVCenter);

    cardLayout->addWidget(titleRow);
    cardLayout->addWidget(m_cardSubtitleLabel);

    m_serverFieldContainer = new QWidget(m_card);
    m_serverFieldContainer->setObjectName("LoginServerFieldContainer");
    m_serverFieldContainer->setAttribute(Qt::WA_StyledBackground, true);
    auto *serverFieldLayout = new QVBoxLayout(m_serverFieldContainer);
    serverFieldLayout->setContentsMargins(0, 0, 0, 0);
    serverFieldLayout->setSpacing(4);

    auto *serverLabel = new QLabel(QStringLiteral("Server IP Address"), m_serverFieldContainer);
    serverLabel->setProperty("loginRole", "fieldLabel");
    serverFieldLayout->addWidget(serverLabel);

    m_serverUrlEdit = new QLineEdit(m_serverFieldContainer);
    m_serverUrlEdit->setPlaceholderText(QStringLiteral("192.168.0.110"));
    m_serverUrlEdit->setText(extractServerHost(ConfigManager::instance().getLoginServerUrl()));
    serverFieldLayout->addWidget(createFieldShell(m_serverFieldContainer, QStringLiteral("IP"), m_serverUrlEdit));
    cardLayout->addWidget(m_serverFieldContainer);

    m_formStack = new QStackedWidget(m_card);
    m_formStack->setObjectName("LoginFormStack");
    m_formStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_loginPage = new QWidget(m_formStack);
    m_loginPage->setObjectName("LoginPage");
    m_loginPage->setAttribute(Qt::WA_StyledBackground, true);
    m_loginPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *loginLayout = new QVBoxLayout(m_loginPage);
    loginLayout->setContentsMargins(0, 0, 0, 8);
    loginLayout->setSpacing(10);

    auto *userLabel = new QLabel(QStringLiteral("ID"), m_loginPage);
    userLabel->setProperty("loginRole", "fieldLabel");
    loginLayout->addWidget(userLabel);

    m_userIdEdit = new QLineEdit(m_loginPage);
    m_userIdEdit->setPlaceholderText(QStringLiteral("admin"));
    m_userIdEdit->setText(ConfigManager::instance().getRememberedUserId());
    loginLayout->addWidget(createFieldShell(m_loginPage, QStringLiteral("ID"), m_userIdEdit));

    auto *passwordLabel = new QLabel(QStringLiteral("Password"), m_loginPage);
    passwordLabel->setProperty("loginRole", "fieldLabel");
    loginLayout->addWidget(passwordLabel);

    m_passwordEdit = new QLineEdit(m_loginPage);
    m_passwordEdit->setPlaceholderText(QStringLiteral("Enter your password"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);

    m_passwordToggleButton = new QPushButton(QStringLiteral("SHOW"), m_loginPage);
    m_passwordToggleButton->setObjectName("LoginPasswordToggle");
    m_passwordToggleButton->setCursor(Qt::PointingHandCursor);
    m_passwordToggleButton->setFixedHeight(30);
    connect(m_passwordToggleButton, &QPushButton::clicked, this, &LoginDialog::togglePasswordVisibility);

    loginLayout->addWidget(createFieldShell(m_loginPage, QStringLiteral("PW"), m_passwordEdit, m_passwordToggleButton));

    auto *optionsLayout = new QHBoxLayout();
    optionsLayout->setContentsMargins(0, 0, 0, 0);
    optionsLayout->setSpacing(12);

    m_rememberCheck = new QCheckBox(QStringLiteral("Remember device"), m_loginPage);
    m_rememberCheck->setObjectName("LoginRememberCheck");
    m_rememberCheck->setChecked(ConfigManager::instance().getRememberUser());

    m_helpButton = new QPushButton(QStringLiteral("Forgot Password?"), m_loginPage);
    m_helpButton->setObjectName("LoginHelpButton");
    m_helpButton->setCursor(Qt::PointingHandCursor);
    connect(m_helpButton, &QPushButton::clicked, this, &LoginDialog::showForgotPasswordHelp);

    optionsLayout->addWidget(m_rememberCheck);
    optionsLayout->addStretch();
    optionsLayout->addWidget(m_helpButton, 0, Qt::AlignVCenter);
    loginLayout->addLayout(optionsLayout);

    m_submitButton = new QPushButton(m_loginPage);
    m_submitButton->setObjectName("LoginSubmitButton");
    m_submitButton->setCursor(Qt::PointingHandCursor);
    m_submitButton->setFixedHeight(46);
    connect(m_submitButton, &QPushButton::clicked, this, &LoginDialog::attemptLogin);
    loginLayout->addWidget(m_submitButton);

    m_signUpButton = new QPushButton(QStringLiteral("Create Account"), m_loginPage);
    m_signUpButton->setObjectName("LoginSecondaryButton");
    m_signUpButton->setCursor(Qt::PointingHandCursor);
    m_signUpButton->setFixedHeight(42);
    connect(m_signUpButton, &QPushButton::clicked, this, &LoginDialog::showSignUpPage);
    loginLayout->addWidget(m_signUpButton);
    m_formStack->addWidget(m_loginPage);

    m_signUpPage = new QWidget(m_formStack);
    m_signUpPage->setObjectName("LoginSignUpPage");
    m_signUpPage->setAttribute(Qt::WA_StyledBackground, true);
    auto *signUpLayout = new QVBoxLayout(m_signUpPage);
    signUpLayout->setContentsMargins(0, 0, 0, 0);
    signUpLayout->setSpacing(8);

    auto createSignUpGroup = [this](const QString &labelText,
                                    const QString &tag,
                                    QLineEdit **target,
                                    const QString &placeholder) {
        auto *group = new QWidget(m_signUpPage);
        group->setProperty("loginRole", "panel");
        group->setAttribute(Qt::WA_StyledBackground, true);
        group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto *groupLayout = new QVBoxLayout(group);
        groupLayout->setContentsMargins(0, 0, 0, 0);
        groupLayout->setSpacing(4);

        auto *label = new QLabel(labelText, group);
        label->setProperty("loginRole", "fieldLabel");
        groupLayout->addWidget(label);

        *target = new QLineEdit(group);
        (*target)->setPlaceholderText(placeholder);
        groupLayout->addWidget(createFieldShell(group, tag, *target));

        return group;
    };

    m_signUpStepStack = new QStackedWidget(m_signUpPage);
    m_signUpStepStack->setObjectName("SignUpStepStack");
    m_signUpStepStack->setAttribute(Qt::WA_StyledBackground, true);
    m_signUpStepStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_signUpStepOnePage = new QWidget(m_signUpStepStack);
    m_signUpStepOnePage->setObjectName("LoginSignUpStepOnePage");
    m_signUpStepOnePage->setAttribute(Qt::WA_StyledBackground, true);
    m_signUpStepOnePage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *signUpStepOneLayout = new QVBoxLayout(m_signUpStepOnePage);
    signUpStepOneLayout->setContentsMargins(0, 0, 0, 0);
    signUpStepOneLayout->setSpacing(8);

    auto *nameGroup = createSignUpGroup(QStringLiteral("Display Name"),
                                        QStringLiteral("NAME"),
                                        &m_signUpNameEdit,
                                        QStringLiteral("Administrator"));
    signUpStepOneLayout->addWidget(nameGroup);

    auto *emailGroup = createSignUpGroup(QStringLiteral("Email Address"),
                                         QStringLiteral("MAIL"),
                                         &m_signUpEmailEdit,
                                         QStringLiteral("admin@example.com"));
    signUpStepOneLayout->addWidget(emailGroup);

    m_signUpNextButton = new QPushButton(QStringLiteral("NEXT"), m_signUpStepOnePage);
    m_signUpNextButton->setObjectName("LoginSubmitButton");
    m_signUpNextButton->setCursor(Qt::PointingHandCursor);
    m_signUpNextButton->setFixedHeight(46);
    connect(m_signUpNextButton, &QPushButton::clicked, this, &LoginDialog::advanceSignUpStep);

    m_backButton = new QPushButton(QStringLiteral("Back To Sign In"), m_signUpStepOnePage);
    m_backButton->setObjectName("LoginSecondaryButton");
    m_backButton->setCursor(Qt::PointingHandCursor);
    m_backButton->setFixedHeight(42);
    connect(m_backButton, &QPushButton::clicked, this, &LoginDialog::showLoginPage);

    auto *signUpStepOneActionRow = new QHBoxLayout();
    signUpStepOneActionRow->setContentsMargins(0, 2, 0, 0);
    signUpStepOneActionRow->setSpacing(10);
    signUpStepOneActionRow->addWidget(m_backButton);
    signUpStepOneActionRow->addWidget(m_signUpNextButton, 1);
    signUpStepOneLayout->addLayout(signUpStepOneActionRow);
    m_signUpStepStack->addWidget(m_signUpStepOnePage);

    m_signUpStepTwoPage = new QWidget(m_signUpStepStack);
    m_signUpStepTwoPage->setObjectName("LoginSignUpStepTwoPage");
    m_signUpStepTwoPage->setAttribute(Qt::WA_StyledBackground, true);
    m_signUpStepTwoPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *signUpStepTwoLayout = new QVBoxLayout(m_signUpStepTwoPage);
    signUpStepTwoLayout->setContentsMargins(0, 0, 0, 0);
    signUpStepTwoLayout->setSpacing(8);

    auto *identityGroup = createSignUpGroup(QStringLiteral("ID"),
                                            QStringLiteral("ID"),
                                            &m_signUpUserIdEdit,
                                            QStringLiteral("admin"));
    signUpStepTwoLayout->addWidget(identityGroup);

    auto *passwordGroup = createSignUpGroup(QStringLiteral("Account Password"),
                                            QStringLiteral("PW"),
                                            &m_signUpPasswordEdit,
                                            QStringLiteral("Choose password"));
    signUpStepTwoLayout->addWidget(passwordGroup);

    auto *confirmPasswordGroup = createSignUpGroup(QStringLiteral("Confirm Password"),
                                                   QStringLiteral("CONF"),
                                                   &m_signUpConfirmPasswordEdit,
                                                   QStringLiteral("Repeat password"));
    signUpStepTwoLayout->addWidget(confirmPasswordGroup);

    m_signUpPasswordEdit->setEchoMode(QLineEdit::Password);
    m_signUpConfirmPasswordEdit->setEchoMode(QLineEdit::Password);

    m_registerButton = new QPushButton(m_signUpStepTwoPage);
    m_registerButton->setObjectName("LoginSubmitButton");
    m_registerButton->setCursor(Qt::PointingHandCursor);
    m_registerButton->setFixedHeight(46);
    connect(m_registerButton, &QPushButton::clicked, this, &LoginDialog::attemptRegistration);

    m_signUpPrevButton = new QPushButton(QStringLiteral("BACK"), m_signUpStepTwoPage);
    m_signUpPrevButton->setObjectName("LoginSecondaryButton");
    m_signUpPrevButton->setCursor(Qt::PointingHandCursor);
    m_signUpPrevButton->setFixedHeight(42);
    connect(m_signUpPrevButton, &QPushButton::clicked, this, [this]() {
        if (m_busy || !m_signUpStepStack) {
            return;
        }

        m_signUpStepStack->setCurrentWidget(m_signUpStepOnePage);
        applyCurrentPageText();
        updateStatusMessage(QString(), false);
        recalculateDialogSize();
        if (m_signUpNameEdit->text().trimmed().isEmpty()) {
            m_signUpNameEdit->setFocus();
        } else {
            m_signUpEmailEdit->setFocus();
        }
    });

    auto *signUpStepTwoActionRow = new QHBoxLayout();
    signUpStepTwoActionRow->setContentsMargins(0, 2, 0, 0);
    signUpStepTwoActionRow->setSpacing(10);
    signUpStepTwoActionRow->addWidget(m_signUpPrevButton);
    signUpStepTwoActionRow->addWidget(m_registerButton, 1);
    signUpStepTwoLayout->addLayout(signUpStepTwoActionRow);
    m_signUpStepStack->addWidget(m_signUpStepTwoPage);

    signUpLayout->addWidget(m_signUpStepStack);
    m_formStack->addWidget(m_signUpPage);
    m_signUpPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_passwordResetPage = new QWidget(m_formStack);
    m_passwordResetPage->setObjectName("LoginPasswordResetPage");
    m_passwordResetPage->setAttribute(Qt::WA_StyledBackground, true);
    m_passwordResetPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *passwordResetLayout = new QVBoxLayout(m_passwordResetPage);
    passwordResetLayout->setContentsMargins(0, 0, 0, 0);
    passwordResetLayout->setSpacing(8);

    auto createPasswordResetGroup = [this](const QString &labelText,
                                           const QString &tag,
                                           QLineEdit **target,
                                           const QString &placeholder) {
        auto *group = new QWidget(m_passwordResetPage);
        group->setProperty("loginRole", "panel");
        group->setAttribute(Qt::WA_StyledBackground, true);
        group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto *groupLayout = new QVBoxLayout(group);
        groupLayout->setContentsMargins(0, 0, 0, 0);
        groupLayout->setSpacing(4);

        auto *label = new QLabel(labelText, group);
        label->setProperty("loginRole", "fieldLabel");
        groupLayout->addWidget(label);

        *target = new QLineEdit(group);
        (*target)->setPlaceholderText(placeholder);
        groupLayout->addWidget(createFieldShell(group, tag, *target));

        return group;
    };

    auto *resetUserIdGroup = createPasswordResetGroup(QStringLiteral("ID"),
                                                      QStringLiteral("ID"),
                                                      &m_resetUserIdEdit,
                                                      QStringLiteral("admin"));
    passwordResetLayout->addWidget(resetUserIdGroup);

    auto *resetNameGroup = createPasswordResetGroup(QStringLiteral("Display Name"),
                                                    QStringLiteral("NAME"),
                                                    &m_resetNameEdit,
                                                    QStringLiteral("Administrator"));
    passwordResetLayout->addWidget(resetNameGroup);

    auto *resetEmailGroup = createPasswordResetGroup(QStringLiteral("Email Address"),
                                                     QStringLiteral("MAIL"),
                                                     &m_resetEmailEdit,
                                                     QStringLiteral("admin@example.com"));
    passwordResetLayout->addWidget(resetEmailGroup);

    auto *resetPasswordGroup = createPasswordResetGroup(QStringLiteral("New Password"),
                                                        QStringLiteral("NEW"),
                                                        &m_resetPasswordEdit,
                                                        QStringLiteral("Choose a new password"));
    passwordResetLayout->addWidget(resetPasswordGroup);

    auto *resetConfirmGroup = createPasswordResetGroup(QStringLiteral("Confirm Password"),
                                                       QStringLiteral("CONF"),
                                                       &m_resetConfirmPasswordEdit,
                                                       QStringLiteral("Repeat the new password"));
    passwordResetLayout->addWidget(resetConfirmGroup);

    m_resetPasswordEdit->setEchoMode(QLineEdit::Password);
    m_resetConfirmPasswordEdit->setEchoMode(QLineEdit::Password);

    auto *passwordResetActionRow = new QHBoxLayout();
    passwordResetActionRow->setContentsMargins(0, 2, 0, 0);
    passwordResetActionRow->setSpacing(10);

    m_resetBackButton = new QPushButton(QStringLiteral("Back To Sign In"), m_passwordResetPage);
    m_resetBackButton->setObjectName("LoginSecondaryButton");
    m_resetBackButton->setCursor(Qt::PointingHandCursor);
    m_resetBackButton->setFixedHeight(42);
    connect(m_resetBackButton, &QPushButton::clicked, this, &LoginDialog::showLoginPage);
    passwordResetActionRow->addWidget(m_resetBackButton);

    m_resetButton = new QPushButton(m_passwordResetPage);
    m_resetButton->setObjectName("LoginSubmitButton");
    m_resetButton->setCursor(Qt::PointingHandCursor);
    m_resetButton->setFixedHeight(46);
    connect(m_resetButton, &QPushButton::clicked, this, &LoginDialog::attemptPasswordReset);
    passwordResetActionRow->addWidget(m_resetButton, 1);

    passwordResetLayout->addLayout(passwordResetActionRow);

    m_formStack->addWidget(m_passwordResetPage);

    cardLayout->addWidget(m_formStack);

    m_statusLabel = new QLabel(m_card);
    m_statusLabel->setObjectName("LoginStatusLabel");
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_statusLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_statusLabel->setFixedHeight(36);
    cardLayout->addWidget(m_statusLabel);

    auto *divider = new QFrame(m_card);
    divider->setObjectName("LoginFootDivider");
    divider->setFrameShape(QFrame::HLine);
    cardLayout->addWidget(divider);

    auto *healthLayout = new QHBoxLayout();
    healthLayout->setContentsMargins(0, 0, 0, 0);
    healthLayout->setSpacing(8);
    healthLayout->setAlignment(Qt::AlignCenter);

    m_healthDot = new QLabel(m_card);
    m_healthDot->setObjectName("LoginHealthDot");
    m_healthDot->setFixedSize(10, 10);

    m_healthLabel = new QLabel(QStringLiteral("System Status: Operational"), m_card);
    m_healthLabel->setObjectName("LoginHealthLabel");

    healthLayout->addWidget(m_healthDot);
    healthLayout->addWidget(m_healthLabel);
    cardLayout->addLayout(healthLayout);

    rootLayout->addWidget(m_card, 0, Qt::AlignHCenter);

    mainLayout->addWidget(m_root, 0, Qt::AlignCenter);

    connect(m_userIdEdit, &QLineEdit::returnPressed, this, &LoginDialog::attemptLogin);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LoginDialog::attemptLogin);
    connect(m_signUpNameEdit, &QLineEdit::returnPressed, this, &LoginDialog::advanceSignUpStep);
    connect(m_signUpEmailEdit, &QLineEdit::returnPressed, this, &LoginDialog::advanceSignUpStep);
    connect(m_signUpUserIdEdit, &QLineEdit::returnPressed, this, &LoginDialog::attemptRegistration);
    connect(m_signUpPasswordEdit, &QLineEdit::returnPressed, this, &LoginDialog::attemptRegistration);
    connect(m_signUpConfirmPasswordEdit, &QLineEdit::returnPressed, this, &LoginDialog::attemptRegistration);
    connect(m_resetUserIdEdit, &QLineEdit::returnPressed, this, &LoginDialog::attemptPasswordReset);
    connect(m_resetNameEdit, &QLineEdit::returnPressed, this, &LoginDialog::attemptPasswordReset);
    connect(m_resetEmailEdit, &QLineEdit::returnPressed, this, &LoginDialog::attemptPasswordReset);
    connect(m_resetPasswordEdit, &QLineEdit::returnPressed, this, &LoginDialog::attemptPasswordReset);
    connect(m_resetConfirmPasswordEdit, &QLineEdit::returnPressed, this, &LoginDialog::attemptPasswordReset);
    connect(&AuthManager::instance(), &AuthManager::loginSucceeded, this, &LoginDialog::handleLoginSucceeded);
    connect(&AuthManager::instance(), &AuthManager::loginFailed, this, &LoginDialog::handleLoginFailed);
    connect(&AuthManager::instance(), &AuthManager::registrationSucceeded, this, &LoginDialog::handleRegistrationSucceeded);
    connect(&AuthManager::instance(), &AuthManager::registrationFailed, this, &LoginDialog::handleRegistrationFailed);
    connect(&AuthManager::instance(), &AuthManager::passwordResetSucceeded, this, &LoginDialog::handlePasswordResetSucceeded);
    connect(&AuthManager::instance(), &AuthManager::passwordResetFailed, this, &LoginDialog::handlePasswordResetFailed);

    m_root->installEventFilter(this);
    m_card->installEventFilter(this);
    m_cardAccent->installEventFilter(this);
    m_serverFieldContainer->installEventFilter(this);
    m_formStack->installEventFilter(this);
    m_loginPage->installEventFilter(this);
    m_signUpPage->installEventFilter(this);
    m_passwordResetPage->installEventFilter(this);
    m_signUpStepStack->installEventFilter(this);
    m_signUpStepOnePage->installEventFilter(this);
    m_signUpStepTwoPage->installEventFilter(this);
    m_cardTitleLabel->installEventFilter(this);
    m_cardSubtitleLabel->installEventFilter(this);
    m_statusLabel->installEventFilter(this);
    m_healthDot->installEventFilter(this);
    m_healthLabel->installEventFilter(this);

    m_signUpStepStack->setCurrentWidget(m_signUpStepOnePage);
    m_formStack->setCurrentWidget(m_loginPage);
    applyTheme();
    updateStatusMessage(QString(), false);
    recalculateDialogSize();

    if (m_userIdEdit->text().trimmed().isEmpty()) {
        m_userIdEdit->setFocus();
    } else {
        m_passwordEdit->setFocus();
    }
}

bool LoginDialog::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            beginWindowDrag(globalMousePosition(mouseEvent));
            return true;
        }
        break;
    }
    case QEvent::MouseMove: {
        if (!m_draggingWindow) {
            break;
        }

        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->buttons() & Qt::LeftButton) {
            updateWindowDrag(globalMousePosition(mouseEvent));
            return true;
        }

        endWindowDrag();
        break;
    }
    case QEvent::MouseButtonRelease: {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && m_draggingWindow) {
            endWindowDrag();
            return true;
        }
        break;
    }
    default:
        break;
    }

    return QDialog::eventFilter(watched, event);
}

void LoginDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        beginWindowDrag(globalMousePosition(event));
        event->accept();
        return;
    }

    QDialog::mousePressEvent(event);
}

void LoginDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (m_draggingWindow && (event->buttons() & Qt::LeftButton)) {
        updateWindowDrag(globalMousePosition(event));
        event->accept();
        return;
    }

    QDialog::mouseMoveEvent(event);
}

void LoginDialog::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_draggingWindow) {
        endWindowDrag();
        event->accept();
        return;
    }

    QDialog::mouseReleaseEvent(event);
}

void LoginDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_isDark) {
        QLinearGradient background(0, 0, width(), height());
        background.setColorAt(0.0, QColor("#08111f"));
        background.setColorAt(0.55, QColor("#0b1326"));
        background.setColorAt(1.0, QColor("#102140"));
        painter.fillRect(rect(), background);

        painter.setPen(QPen(QColor(173, 198, 255, 48), 1.2));
        painter.drawLine(36, 36, 160, 36);
        painter.drawLine(36, 36, 36, 160);
        painter.drawLine(width() - 36, height() - 36, width() - 160, height() - 36);
        painter.drawLine(width() - 36, height() - 36, width() - 36, height() - 160);

        painter.setPen(QPen(QColor(53, 125, 241, 35), 1.0));
        painter.drawLine(48, height() / 2, 180, height() / 2);
        painter.drawLine(width() - 48, height() / 2, width() - 180, height() / 2);
    } else {
        QLinearGradient background(0, 0, width(), height());
        background.setColorAt(0.0, QColor("#fffdf8"));
        background.setColorAt(1.0, QColor("#eef6ff"));
        painter.fillRect(rect(), background);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(196, 198, 209, 95));
        for (int y = 20; y < height(); y += 38) {
            for (int x = 20; x < width(); x += 38) {
                painter.drawEllipse(QPointF(x, y), 1.25, 1.25);
            }
        }

    }
}

void LoginDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);

    if (m_cardAccent && m_card) {
        m_cardAccent->setGeometry(m_card->width() - 54, -18, 44, 44);
    }

    syncFormWidths();
}

void LoginDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    syncFormWidths();
    positionDialog();
}

void LoginDialog::attemptLogin()
{
    if (m_busy) {
        return;
    }

    const QString userId = m_userIdEdit->text().trimmed();
    const QString password = m_passwordEdit->text();

    // Allow a local master override that skips the JWT request and opens the client directly.
    if (isLocalMasterLogin(userId, password)) {
        AuthManager::instance().clearSession();
        ConfigManager::instance().setDarkTheme(m_isDark);
        ConfigManager::instance().setRememberUser(m_rememberCheck->isChecked());
        ConfigManager::instance().setRememberedUserId(m_rememberCheck->isChecked() ? userId : QString());
        ConfigManager::instance().setActiveUserId(userId);
        ConfigManager::instance().setActiveUserEmail(QString());
        ConfigManager::instance().setActiveAuthMode(QStringLiteral("Local Master"));

        bool hostOk = false;
        const QString serverHost = normalizedServerHost(&hostOk);
        ConfigManager::instance().setLoginServerUrl(hostOk ? serverHost : m_serverUrlEdit->text().trimmed());
        accept();
        return;
    }

    bool ok = false;
    const QString baseUrl = normalizedBaseUrl(&ok);
    if (!ok) {
        updateStatusMessage(QStringLiteral("Enter a valid server IP, for example 192.168.0.110."), true);
        m_serverUrlEdit->setFocus();
        return;
    }

    if (userId.isEmpty()) {
        updateStatusMessage(QStringLiteral("Operator ID is required."), true);
        m_userIdEdit->setFocus();
        return;
    }

    if (password.isEmpty()) {
        updateStatusMessage(QStringLiteral("Password is required."), true);
        m_passwordEdit->setFocus();
        return;
    }

    m_serverUrlEdit->setText(normalizedServerHost());
    setBusy(true);
    updateStatusMessage(QStringLiteral("Contacting authentication gateway and requesting JWT session..."));
    AuthManager::instance().login(baseUrl, userId, password);
}

void LoginDialog::advanceSignUpStep()
{
    if (m_busy || !m_signUpStepStack) {
        return;
    }

    bool ok = false;
    const QString baseUrl = normalizedBaseUrl(&ok);
    if (!ok) {
        updateStatusMessage(QStringLiteral("Enter a valid server IP before continuing."), true);
        m_serverUrlEdit->setFocus();
        return;
    }

    const QString name = m_signUpNameEdit->text().trimmed();
    const QString email = m_signUpEmailEdit->text().trimmed();

    if (name.isEmpty()) {
        updateStatusMessage(QStringLiteral("Display name is required."), true);
        m_signUpNameEdit->setFocus();
        return;
    }

    if (!looksLikeEmail(email)) {
        updateStatusMessage(QStringLiteral("Enter a valid email address."), true);
        m_signUpEmailEdit->setFocus();
        return;
    }

    m_serverUrlEdit->setText(normalizedServerHost());
    resetSignUpStepTwoInputs();
    m_signUpStepStack->setCurrentWidget(m_signUpStepTwoPage);
    applyCurrentPageText();
    updateStatusMessage(QString(), false);
    recalculateDialogSize();
    m_signUpUserIdEdit->setFocus();
}

void LoginDialog::attemptRegistration()
{
    if (m_busy) {
        return;
    }

    const QString userId = m_signUpUserIdEdit->text().trimmed();
    const QString name = m_signUpNameEdit->text().trimmed();
    const QString email = m_signUpEmailEdit->text().trimmed();
    const QString password = m_signUpPasswordEdit->text();
    const QString confirmPassword = m_signUpConfirmPasswordEdit->text();

    bool ok = false;
    const QString baseUrl = normalizedBaseUrl(&ok);
    if (!ok) {
        updateStatusMessage(QStringLiteral("Enter a valid server IP before creating an account."), true);
        m_serverUrlEdit->setFocus();
        return;
    }

    if (userId.isEmpty()) {
        updateStatusMessage(QStringLiteral("Operator ID is required."), true);
        m_signUpUserIdEdit->setFocus();
        return;
    }

    if (name.isEmpty()) {
        updateStatusMessage(QStringLiteral("Display name is required."), true);
        m_signUpNameEdit->setFocus();
        return;
    }

    if (!looksLikeEmail(email)) {
        updateStatusMessage(QStringLiteral("Enter a valid email address."), true);
        m_signUpEmailEdit->setFocus();
        return;
    }

    if (password.isEmpty()) {
        updateStatusMessage(QStringLiteral("Password is required."), true);
        m_signUpPasswordEdit->setFocus();
        return;
    }

    if (password != confirmPassword) {
        updateStatusMessage(QStringLiteral("The password confirmation does not match."), true);
        m_signUpConfirmPasswordEdit->selectAll();
        m_signUpConfirmPasswordEdit->setFocus();
        return;
    }

    m_serverUrlEdit->setText(normalizedServerHost());
    setBusy(true);
    updateStatusMessage(QStringLiteral("Submitting operator enrollment request to POST /users..."));
    AuthManager::instance().registerUser(baseUrl, userId, name, email, password);
}

void LoginDialog::handleLoginSucceeded()
{
    ConfigManager::instance().setLoginServerUrl(m_serverUrlEdit->text().trimmed());
    ConfigManager::instance().setDarkTheme(m_isDark);
    ConfigManager::instance().setRememberUser(m_rememberCheck->isChecked());
    ConfigManager::instance().setActiveUserId(m_userIdEdit->text().trimmed());
    ConfigManager::instance().setActiveUserEmail(QString());
    ConfigManager::instance().setActiveAuthMode(QStringLiteral("JWT Session"));
    if (m_rememberCheck->isChecked()) {
        ConfigManager::instance().setRememberedUserId(m_userIdEdit->text().trimmed());
    }

    updateStatusMessage(QStringLiteral("Access granted. Initializing secure monitoring session..."));
    setBusy(false);
    accept();
}

void LoginDialog::handleLoginFailed(const QString &message)
{
    setBusy(false);
    updateStatusMessage(message.isEmpty() ? QStringLiteral("Login failed.") : message, true);
    m_passwordEdit->selectAll();
    m_passwordEdit->setFocus();
}

void LoginDialog::handleRegistrationSucceeded(const QString &userId)
{
    setBusy(false);

    m_userIdEdit->setText(userId.trimmed());
    m_passwordEdit->clear();
    m_signUpNameEdit->clear();
    m_signUpEmailEdit->clear();
    resetSignUpStepTwoInputs();
    m_signUpStepStack->setCurrentWidget(m_signUpStepOnePage);

    showLoginPage();
    updateStatusMessage(QStringLiteral("Account created. Sign in with the new credentials."));
    m_passwordEdit->setFocus();
}

void LoginDialog::handleRegistrationFailed(const QString &message)
{
    setBusy(false);
    updateStatusMessage(message.isEmpty() ? QStringLiteral("Registration failed.") : message, true);
    m_signUpConfirmPasswordEdit->setFocus();
}

void LoginDialog::attemptPasswordReset()
{
    if (m_busy) {
        return;
    }

    bool ok = false;
    const QString baseUrl = normalizedBaseUrl(&ok);
    if (!ok) {
        updateStatusMessage(QStringLiteral("Enter a valid server IP before resetting the password."), true);
        m_serverUrlEdit->setFocus();
        return;
    }

    const QString userId = m_resetUserIdEdit->text().trimmed();
    const QString name = m_resetNameEdit->text().trimmed();
    const QString email = m_resetEmailEdit->text().trimmed();
    const QString newPassword = m_resetPasswordEdit->text();
    const QString confirmPassword = m_resetConfirmPasswordEdit->text();

    if (userId.isEmpty()) {
        updateStatusMessage(QStringLiteral("Operator ID is required."), true);
        m_resetUserIdEdit->setFocus();
        return;
    }

    if (name.isEmpty()) {
        updateStatusMessage(QStringLiteral("Display name is required."), true);
        m_resetNameEdit->setFocus();
        return;
    }

    if (!looksLikeEmail(email)) {
        updateStatusMessage(QStringLiteral("Enter a valid email address."), true);
        m_resetEmailEdit->setFocus();
        return;
    }

    if (newPassword.isEmpty()) {
        updateStatusMessage(QStringLiteral("Enter a new password."), true);
        m_resetPasswordEdit->setFocus();
        return;
    }

    if (newPassword != confirmPassword) {
        updateStatusMessage(QStringLiteral("The password confirmation does not match."), true);
        m_resetConfirmPasswordEdit->selectAll();
        m_resetConfirmPasswordEdit->setFocus();
        return;
    }

    m_serverUrlEdit->setText(normalizedServerHost());
    setBusy(true);
    updateStatusMessage(QStringLiteral("Verifying account details and updating the password..."));
    AuthManager::instance().resetPassword(baseUrl, userId, name, email, newPassword);
}

void LoginDialog::handlePasswordResetSucceeded(const QString &userId)
{
    setBusy(false);

    m_userIdEdit->setText(userId.trimmed());
    m_passwordEdit->clear();
    resetPasswordResetInputs();
    showLoginPage();
    updateStatusMessage(QStringLiteral("Password updated. Sign in with the new password."));
    m_passwordEdit->setFocus();
}

void LoginDialog::handlePasswordResetFailed(const QString &message)
{
    setBusy(false);
    updateStatusMessage(message.isEmpty() ? QStringLiteral("Password reset failed.") : message, true);
    if (m_resetConfirmPasswordEdit) {
        m_resetConfirmPasswordEdit->selectAll();
        m_resetConfirmPasswordEdit->setFocus();
    }
}

void LoginDialog::showSignUpPage()
{
    if (m_busy) {
        return;
    }

    if (!m_serverUrlEdit->text().trimmed().isEmpty()) {
        bool ok = false;
        const QString normalized = normalizedServerHost(&ok);
        if (ok) {
            m_serverUrlEdit->setText(normalized);
        }
    }

    resetSignUpStepTwoInputs();
    m_signUpStepStack->setCurrentWidget(m_signUpStepOnePage);
    m_formStack->setCurrentWidget(m_signUpPage);
    applyCurrentPageText();
    updateStatusMessage(QString(), false);
    recalculateDialogSize();
    if (m_signUpNameEdit->text().trimmed().isEmpty()) {
        m_signUpNameEdit->setFocus();
    } else {
        m_signUpEmailEdit->setFocus();
    }
}

void LoginDialog::resetSignUpStepTwoInputs()
{
    if (m_signUpUserIdEdit) {
        m_signUpUserIdEdit->clear();
    }
    if (m_signUpPasswordEdit) {
        m_signUpPasswordEdit->clear();
        m_signUpPasswordEdit->setEchoMode(QLineEdit::Password);
    }
    if (m_signUpConfirmPasswordEdit) {
        m_signUpConfirmPasswordEdit->clear();
        m_signUpConfirmPasswordEdit->setEchoMode(QLineEdit::Password);
    }
}

void LoginDialog::resetPasswordResetInputs()
{
    if (m_resetNameEdit) {
        m_resetNameEdit->clear();
    }
    if (m_resetEmailEdit) {
        m_resetEmailEdit->clear();
    }
    if (m_resetPasswordEdit) {
        m_resetPasswordEdit->clear();
        m_resetPasswordEdit->setEchoMode(QLineEdit::Password);
    }
    if (m_resetConfirmPasswordEdit) {
        m_resetConfirmPasswordEdit->clear();
        m_resetConfirmPasswordEdit->setEchoMode(QLineEdit::Password);
    }
}

void LoginDialog::showLoginPage()
{
    if (m_busy) {
        return;
    }

    m_formStack->setCurrentWidget(m_loginPage);
    applyCurrentPageText();
    updateStatusMessage(QString(), false);
    recalculateDialogSize();
    if (m_userIdEdit->text().trimmed().isEmpty()) {
        m_userIdEdit->setFocus();
    } else {
        m_passwordEdit->setFocus();
    }
}

void LoginDialog::toggleTheme()
{
    m_isDark = !m_isDark;
    ConfigManager::instance().setDarkTheme(m_isDark);
    applyTheme();
    recalculateDialogSize();
    update();
}

void LoginDialog::togglePasswordVisibility()
{
    const bool showPlainText = (m_passwordEdit->echoMode() == QLineEdit::Password);
    m_passwordEdit->setEchoMode(showPlainText ? QLineEdit::Normal : QLineEdit::Password);
    m_passwordToggleButton->setText(showPlainText ? QStringLiteral("HIDE") : QStringLiteral("SHOW"));
}

void LoginDialog::showForgotPasswordHelp()
{
    if (m_busy) {
        return;
    }

    if (!m_serverUrlEdit->text().trimmed().isEmpty()) {
        bool ok = false;
        const QString normalized = normalizedServerHost(&ok);
        if (ok) {
            m_serverUrlEdit->setText(normalized);
        }
    }

    m_resetUserIdEdit->setText(m_userIdEdit->text().trimmed());
    resetPasswordResetInputs();
    m_formStack->setCurrentWidget(m_passwordResetPage);
    applyCurrentPageText();
    updateStatusMessage(QString(), false);
    recalculateDialogSize();

    if (m_resetUserIdEdit->text().trimmed().isEmpty()) {
        m_resetUserIdEdit->setFocus();
    } else {
        m_resetNameEdit->setFocus();
    }
}

QWidget *LoginDialog::createFieldShell(QWidget *owner, const QString &tag, QLineEdit *edit, QWidget *trailingWidget)
{
    auto *shell = new QFrame(owner);
    shell->setObjectName("LoginFieldShell");
    shell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    shell->setFixedHeight(56);

    auto *layout = new QHBoxLayout(shell);
    layout->setContentsMargins(14, 8, 12, 8);
    layout->setSpacing(10);

    auto *tagLabel = new QLabel(upperTag(tag), shell);
    tagLabel->setProperty("loginRole", "fieldTag");
    tagLabel->setAlignment(Qt::AlignCenter);
    tagLabel->setMinimumWidth(40);

    edit->setProperty("loginRole", "fieldEdit");
    edit->setFrame(false);
    edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    edit->setFixedHeight(38);

    layout->addWidget(tagLabel);
    layout->addWidget(edit, 1);

    if (trailingWidget) {
        layout->addWidget(trailingWidget, 0, Qt::AlignVCenter);
    }

    return shell;
}

void LoginDialog::applyTheme()
{
    const QString loginSubmitText = QStringLiteral("LOGIN");
    const QString signUpSubmitText = QStringLiteral("CREATE ACCOUNT");
    const QString resetSubmitText = QStringLiteral("RESET PASSWORD");

    m_submitButton->setText(m_busy && !isSignUpPageActive() ? QStringLiteral("AUTHENTICATING...") : loginSubmitText);
    m_signUpNextButton->setText(QStringLiteral("NEXT"));
    m_registerButton->setText(m_busy && isSignUpPageActive() ? QStringLiteral("CREATING ACCOUNT...") : signUpSubmitText);
    m_resetButton->setText(m_busy && isPasswordResetPageActive() ? QStringLiteral("RESETTING PASSWORD...") : resetSubmitText);
    m_themeButton->setText(m_isDark ? QStringLiteral("LIGHT MODE") : QStringLiteral("DARK MODE"));
    applyCurrentPageText();

    const QString styleSheet = m_isDark
        ? QStringLiteral(R"(
QDialog#LoginDialog {
    color: #dae2fd;
}
#LoginTopButton, #LoginCloseButton {
    background: rgba(23, 31, 51, 0.88);
    color: #adc6ff;
    border: 1px solid rgba(144, 144, 151, 0.25);
    border-radius: 18px;
    padding: 0 16px;
    font-weight: 700;
    letter-spacing: 0.8px;
}
#LoginTopButton:hover {
    background: rgba(45, 52, 73, 0.95);
}
#LoginCloseButton {
    color: #ffb4ab;
}
#LoginCloseButton:hover {
    background: rgba(147, 0, 10, 0.26);
    border-color: rgba(255, 180, 171, 0.22);
}
#LoginCard {
    background: rgba(11, 19, 38, 0.90);
    border: 1px solid rgba(69, 70, 77, 0.45);
    border-radius: 24px;
}
#LoginCardAccent {
    background: transparent;
}
#LoginCardTitle {
    color: #f8fbff;
    font-size: 24px;
    font-weight: 800;
    background: transparent;
}
#LoginCardSubtitle {
    color: rgba(198, 198, 205, 0.82);
    font-size: 13px;
    line-height: 1.5;
    background: transparent;
}
QStackedWidget#LoginFormStack {
    background: transparent;
}
QWidget#LoginServerFieldContainer,
QWidget#LoginPage,
QWidget#LoginSignUpPage,
QWidget#LoginSignUpStepOnePage,
QWidget#LoginSignUpStepTwoPage,
QWidget#LoginPasswordResetPage,
QWidget[loginRole="panel"],
QStackedWidget#SignUpStepStack {
    background: transparent;
}
QLabel[loginRole="fieldLabel"] {
    color: #c6c6cd;
    font-size: 10px;
    font-weight: 800;
    letter-spacing: 2px;
    text-transform: uppercase;
    background: transparent;
    margin-left: 4px;
}
#LoginFieldShell {
    background: rgba(6, 14, 32, 0.95);
    border: 1px solid rgba(69, 70, 77, 0.65);
    border-radius: 16px;
}
#LoginFieldShell:focus-within {
    border: 1px solid #adc6ff;
}
QLabel[loginRole="fieldTag"] {
    color: #909097;
    font-size: 11px;
    font-weight: 800;
    letter-spacing: 1px;
    background: transparent;
}
QLineEdit[loginRole="fieldEdit"] {
    background: transparent;
    color: #edf2ff;
    border: none;
    font-size: 15px;
    font-weight: 600;
    selection-background-color: #357df1;
}
QLineEdit[loginRole="fieldEdit"]::placeholder {
    color: rgba(144, 144, 151, 0.65);
}
#LoginPasswordToggle {
    background: rgba(45, 52, 73, 0.85);
    color: #c6d8ff;
    border: 1px solid rgba(173, 198, 255, 0.18);
    border-radius: 12px;
    padding: 0 14px;
    font-size: 11px;
    font-weight: 800;
    letter-spacing: 1px;
}
#LoginPasswordToggle:hover {
    background: rgba(53, 125, 241, 0.18);
}
#LoginRememberCheck {
    color: #c6c6cd;
    font-size: 13px;
    font-weight: 600;
}
#LoginRememberCheck::indicator {
    width: 18px;
    height: 18px;
    border-radius: 6px;
    border: 1px solid rgba(144, 144, 151, 0.65);
    background: rgba(6, 14, 32, 0.95);
}
#LoginRememberCheck::indicator:checked {
    background: #adc6ff;
    border-color: #adc6ff;
}
#LoginHelpButton {
    background: transparent;
    color: #adc6ff;
    border: none;
    font-size: 13px;
    font-weight: 700;
}
#LoginHelpButton:hover {
    color: #d8e2ff;
}
#LoginSecondaryButton {
    background: rgba(23, 31, 51, 0.88);
    color: #c6c6cd;
    border: 1px solid rgba(69, 70, 77, 0.65);
    border-radius: 14px;
    padding: 0 14px;
    font-size: 12px;
    font-weight: 700;
}
#LoginSecondaryButton:hover {
    background: rgba(45, 52, 73, 0.95);
    color: #edf2ff;
}
#LoginSubmitButton {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #adc6ff, stop:1 #357df1);
    color: #002e6a;
    border: none;
    border-radius: 16px;
    font-size: 16px;
    font-weight: 800;
    letter-spacing: 0.6px;
}
#LoginSubmitButton:hover {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #c4d7ff, stop:1 #4d8df3);
}
#LoginSubmitButton:disabled, #LoginSecondaryButton:disabled, #LoginTopButton:disabled, #LoginCloseButton:disabled, #LoginHelpButton:disabled, #LoginPasswordToggle:disabled {
    background: #41506d;
    color: rgba(237, 242, 255, 0.65);
}
#LoginHelpButton:disabled {
    border: none;
}
#LoginStatusLabel {
    color: #cbd5e1;
    font-size: 12px;
    background: transparent;
}
#LoginFootDivider {
    color: transparent;
    background: rgba(69, 70, 77, 0.40);
    min-height: 1px;
    max-height: 1px;
    border: none;
}
#LoginHealthDot {
    background: #dec29a;
    border-radius: 5px;
}
#LoginHealthLabel {
    color: #dec29a;
    font-size: 10px;
    font-weight: 800;
    letter-spacing: 1.6px;
    background: transparent;
}
QLabel[loginRole="telemetryLabel"] {
    color: rgba(198, 198, 205, 0.70);
    font-size: 9px;
    font-weight: 700;
    letter-spacing: 1.8px;
    background: transparent;
}
QLabel[loginRole="telemetryValue"] {
    color: #edf2ff;
    font-size: 13px;
    font-weight: 800;
    background: transparent;
}
)")
        : QStringLiteral(R"(
QDialog#LoginDialog {
    color: #283044;
}
#LoginTopButton, #LoginCloseButton {
    background: rgba(255, 255, 255, 0.92);
    color: #ff6600;
    border: 1px solid rgba(196, 198, 209, 0.85);
    border-radius: 18px;
    padding: 0 16px;
    font-weight: 800;
    letter-spacing: 0.8px;
}
#LoginTopButton:hover {
    background: #ffffff;
}
#LoginCloseButton {
    color: #ba1a1a;
}
#LoginCloseButton:hover {
    border-color: rgba(186, 26, 26, 0.30);
    background: rgba(255, 218, 214, 0.92);
}
#LoginCard {
    background: rgba(255, 255, 255, 0.96);
    border: 1px solid rgba(196, 198, 209, 0.70);
    border-radius: 24px;
}
#LoginCardAccent {
    background: rgba(255, 102, 0, 0.10);
    border-radius: 0px;
}
#LoginCardTitle {
    color: #191c20;
    font-size: 26px;
    font-weight: 900;
    background: transparent;
}
#LoginCardSubtitle {
    color: rgba(68, 70, 80, 0.88);
    font-size: 13px;
    line-height: 1.5;
    background: transparent;
}
QStackedWidget#LoginFormStack {
    background: transparent;
}
QWidget#LoginServerFieldContainer,
QWidget#LoginPage,
QWidget#LoginSignUpPage,
QWidget#LoginSignUpStepOnePage,
QWidget#LoginSignUpStepTwoPage,
QWidget#LoginPasswordResetPage,
QWidget[loginRole="panel"],
QStackedWidget#SignUpStepStack {
    background: transparent;
}
QLabel[loginRole="fieldLabel"] {
    color: #5c5e6a;
    font-size: 10px;
    font-weight: 800;
    letter-spacing: 2px;
    text-transform: uppercase;
    background: transparent;
    margin-left: 4px;
}
#LoginFieldShell {
    background: #f6f8fc;
    border: 1px solid rgba(196, 198, 209, 0.85);
    border-radius: 16px;
}
#LoginFieldShell:focus-within {
    border: 1px solid #ff6600;
}
QLabel[loginRole="fieldTag"] {
    color: #74777f;
    font-size: 11px;
    font-weight: 800;
    letter-spacing: 1px;
    background: transparent;
}
QLineEdit[loginRole="fieldEdit"] {
    background: transparent;
    color: #191c20;
    border: none;
    font-size: 15px;
    font-weight: 700;
    selection-background-color: #ffb37d;
}
QLineEdit[loginRole="fieldEdit"]::placeholder {
    color: rgba(116, 119, 127, 0.68);
}
#LoginPasswordToggle {
    background: rgba(255, 102, 0, 0.08);
    color: #ff6600;
    border: 1px solid rgba(255, 102, 0, 0.18);
    border-radius: 12px;
    padding: 0 14px;
    font-size: 11px;
    font-weight: 800;
    letter-spacing: 1px;
}
#LoginPasswordToggle:hover {
    background: rgba(255, 102, 0, 0.16);
}
#LoginRememberCheck {
    color: #444650;
    font-size: 13px;
    font-weight: 700;
}
#LoginRememberCheck::indicator {
    width: 18px;
    height: 18px;
    border-radius: 6px;
    border: 1px solid rgba(196, 198, 209, 0.95);
    background: #ffffff;
}
#LoginRememberCheck::indicator:checked {
    background: #ff6600;
    border-color: #ff6600;
}
#LoginHelpButton {
    background: transparent;
    color: #5c5e6a;
    border: none;
    font-size: 13px;
    font-weight: 800;
}
#LoginHelpButton:hover {
    color: #ff6600;
}
#LoginSecondaryButton {
    background: #ffffff;
    color: #444650;
    border: 1px solid rgba(196, 198, 209, 0.95);
    border-radius: 14px;
    padding: 0 14px;
    font-size: 12px;
    font-weight: 800;
}
#LoginSecondaryButton:hover {
    border-color: rgba(255, 102, 0, 0.40);
    color: #ff6600;
}
#LoginSubmitButton {
    background: #ff6600;
    color: white;
    border: none;
    border-radius: 16px;
    font-size: 16px;
    font-weight: 900;
    letter-spacing: 0.6px;
}
#LoginSubmitButton:hover {
    background: #e65c00;
}
#LoginSubmitButton:disabled, #LoginSecondaryButton:disabled, #LoginTopButton:disabled, #LoginCloseButton:disabled, #LoginHelpButton:disabled, #LoginPasswordToggle:disabled {
    background: #d2d8e4;
    color: rgba(25, 28, 32, 0.55);
}
#LoginHelpButton:disabled {
    border: none;
}
#LoginStatusLabel {
    color: #5c5e6a;
    font-size: 12px;
    background: transparent;
}
#LoginFootDivider {
    color: transparent;
    background: rgba(196, 198, 209, 0.55);
    min-height: 1px;
    max-height: 1px;
    border: none;
}
#LoginHealthDot {
    background: #16a34a;
    border-radius: 5px;
}
#LoginHealthLabel {
    color: #444650;
    font-size: 10px;
    font-weight: 900;
    letter-spacing: 1.6px;
    background: transparent;
}
QLabel[loginRole="telemetryLabel"] {
    color: rgba(68, 70, 80, 0.70);
    font-size: 9px;
    font-weight: 800;
    letter-spacing: 1.8px;
    background: transparent;
}
QLabel[loginRole="telemetryValue"] {
    color: #202733;
    font-size: 13px;
    font-weight: 900;
    background: transparent;
}
)");

    setStyleSheet(styleSheet);
    m_cardAccent->setVisible(false);
    m_cardAccent->setGeometry(m_card->width() - 54, -18, 44, 44);
    updateStatusMessage(m_statusLabel->text(), m_statusLabel->property("statusError").toBool());
}

void LoginDialog::applyCurrentPageText()
{
    const bool passwordResetPage = isPasswordResetPageActive();
    const bool signUpPage = isSignUpPageActive();

    m_cardTitleLabel->setText(passwordResetPage
                                  ? (m_isDark ? QStringLiteral("Reset Operator Password") : QStringLiteral("Reset Password"))
                                  : (signUpPage
                                         ? QStringLiteral("누비고 회원가입")
                                         : QStringLiteral("누비고 로그인")));
    m_cardSubtitleLabel->clear();
    m_cardSubtitleLabel->setVisible(false);
    updateServerFieldVisibility();
}

void LoginDialog::syncFormWidths()
{
    if (!m_card || !m_card->layout()) {
        return;
    }

    const QMargins margins = m_card->layout()->contentsMargins();
    const int contentWidth = m_card->width() - margins.left() - margins.right();
    if (contentWidth <= 0) {
        return;
    }

    if (m_formStack) {
        m_formStack->setFixedWidth(contentWidth);
    }
    if (m_loginPage) {
        m_loginPage->setFixedWidth(contentWidth);
    }
    if (m_signUpPage) {
        m_signUpPage->setFixedWidth(contentWidth);
    }
    if (m_passwordResetPage) {
        m_passwordResetPage->setFixedWidth(contentWidth);
    }
    if (m_signUpStepStack) {
        m_signUpStepStack->setFixedWidth(contentWidth);
    }
    if (m_signUpStepOnePage) {
        m_signUpStepOnePage->setFixedWidth(contentWidth);
    }
    if (m_signUpStepTwoPage) {
        m_signUpStepTwoPage->setFixedWidth(contentWidth);
    }
}

void LoginDialog::recalculateDialogSize(bool preserveCurrentHeight)
{
    const int currentSignUpStepHeight = m_signUpStepStack ? m_signUpStepStack->height() : 0;
    const int currentFormStackHeight = m_formStack ? m_formStack->height() : 0;
    QWidget *currentPage = m_formStack ? m_formStack->currentWidget() : nullptr;

    syncFormWidths();

    if (m_signUpStepStack && m_signUpStepStack->currentWidget()) {
        QWidget *signUpStepPage = m_signUpStepStack->currentWidget();
        if (signUpStepPage->layout()) {
            signUpStepPage->layout()->activate();
        }
        const int stepContentHeight = signUpStepPage->layout()
            ? signUpStepPage->layout()->sizeHint().height()
            : signUpStepPage->sizeHint().height();
        int targetSignUpStepHeight = stepContentHeight + 12;
        if (preserveCurrentHeight && currentSignUpStepHeight > 0) {
            targetSignUpStepHeight = qMax(targetSignUpStepHeight, currentSignUpStepHeight);
        }
        m_signUpStepStack->setFixedHeight(targetSignUpStepHeight);
    }

    if (currentPage) {
        if (currentPage->layout()) {
            currentPage->layout()->activate();
        }
        const int pageBuffer = (currentPage == m_loginPage) ? 20 : (currentPage == m_passwordResetPage ? 18 : 8);
        const int pageContentHeight = currentPage->layout()
            ? currentPage->layout()->sizeHint().height()
            : currentPage->sizeHint().height();
        int targetFormStackHeight = pageContentHeight + pageBuffer;
        if (preserveCurrentHeight && currentFormStackHeight > 0) {
            targetFormStackHeight = qMax(targetFormStackHeight, currentFormStackHeight);
        }
        m_formStack->setFixedHeight(targetFormStackHeight);
    }

    layout()->activate();
    adjustSize();
    const int preferredHeight = (currentPage == m_passwordResetPage) ? 760 : 620;
    const QSize preferredSize(560, preferredHeight);
    QSize targetSize = boundedDialogSize(sizeHint().expandedTo(preferredSize));
    if (preserveCurrentHeight && height() > 0) {
        targetSize.setHeight(qMax(targetSize.height(), height()));
    }
    setFixedSize(targetSize);
    syncFormWidths();
    positionDialog();
}

void LoginDialog::beginWindowDrag(const QPoint &globalPos)
{
    if (startSystemWindowMove()) {
        m_draggingWindow = false;
        return;
    }

    m_draggingWindow = true;
    m_dragOffset = globalPos - frameGeometry().topLeft();
}

bool LoginDialog::startSystemWindowMove()
{
    if (!windowHandle()) {
        return false;
    }

    return windowHandle()->startSystemMove();
}

void LoginDialog::updateWindowDrag(const QPoint &globalPos)
{
    if (!m_draggingWindow) {
        return;
    }

    move(globalPos - m_dragOffset);
}

void LoginDialog::endWindowDrag()
{
    m_draggingWindow = false;
}

void LoginDialog::setBusy(bool busy)
{
    m_busy = busy;

    m_serverUrlEdit->setEnabled(!busy);
    m_userIdEdit->setEnabled(!busy);
    m_passwordEdit->setEnabled(!busy);
    m_signUpUserIdEdit->setEnabled(!busy);
    m_signUpNameEdit->setEnabled(!busy);
    m_signUpEmailEdit->setEnabled(!busy);
    m_signUpPasswordEdit->setEnabled(!busy);
    m_signUpConfirmPasswordEdit->setEnabled(!busy);
    m_resetUserIdEdit->setEnabled(!busy);
    m_resetNameEdit->setEnabled(!busy);
    m_resetEmailEdit->setEnabled(!busy);
    m_resetPasswordEdit->setEnabled(!busy);
    m_resetConfirmPasswordEdit->setEnabled(!busy);
    m_rememberCheck->setEnabled(!busy);
    m_helpButton->setEnabled(!busy);
    m_signUpButton->setEnabled(!busy);
    m_signUpNextButton->setEnabled(!busy);
    m_signUpPrevButton->setEnabled(!busy);
    m_registerButton->setEnabled(!busy);
    m_backButton->setEnabled(!busy);
    m_resetButton->setEnabled(!busy);
    m_resetBackButton->setEnabled(!busy);
    m_passwordToggleButton->setEnabled(!busy);
    m_submitButton->setEnabled(!busy);
    m_themeButton->setEnabled(!busy);
    m_closeButton->setEnabled(!busy);

    applyTheme();
}

void LoginDialog::positionDialog()
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

void LoginDialog::updateStatusMessage(const QString &message, bool isError)
{
    m_statusLabel->setProperty("statusError", isError);

    const QString color = isError
        ? (m_isDark ? QStringLiteral("#ffb4ab") : QStringLiteral("#ba1a1a"))
        : (m_isDark ? QStringLiteral("#cbd5e1") : QStringLiteral("#5c5e6a"));

    const bool showStatus = !message.trimmed().isEmpty() && (isError || m_busy);
    m_statusLabel->setText(showStatus ? message : QStringLiteral(" "));
    m_statusLabel->setVisible(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; background: transparent;")
                                     .arg(showStatus ? color : QStringLiteral("transparent")));
}

QString LoginDialog::normalizedBaseUrl(bool *ok) const
{
    bool hostOk = false;
    const QString host = normalizedServerHost(&hostOk);
    if (!hostOk) {
        if (ok) {
            *ok = false;
        }
        return QString();
    }

    QUrl url;
    url.setScheme(QStringLiteral("http"));
    url.setHost(host);
    url.setPort(8080);
    const QString normalized = url.toString(QUrl::RemoveFragment | QUrl::RemoveQuery);

    if (ok) {
        *ok = url.isValid() && !url.host().isEmpty();
    }

    return normalized;
}

QString LoginDialog::normalizedServerHost(bool *ok) const
{
    const QString host = extractServerHost(m_serverUrlEdit ? m_serverUrlEdit->text() : QString());
    if (ok) {
        *ok = !host.isEmpty();
    }
    return host;
}

void LoginDialog::updateServerFieldVisibility()
{
    if (!m_serverFieldContainer) {
        return;
    }

    const bool showServerField = !isSignUpPageActive() || !isSignUpStepTwoActive();
    m_serverFieldContainer->setVisible(showServerField);
}

bool LoginDialog::isSignUpPageActive() const
{
    return m_formStack && (m_formStack->currentWidget() == m_signUpPage);
}

bool LoginDialog::isSignUpStepTwoActive() const
{
    return m_signUpStepStack && (m_signUpStepStack->currentWidget() == m_signUpStepTwoPage);
}

bool LoginDialog::isPasswordResetPageActive() const
{
    return m_formStack && (m_formStack->currentWidget() == m_passwordResetPage);
}
