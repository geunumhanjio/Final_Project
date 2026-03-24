#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QCheckBox>
#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QPoint>
#include <QWidget>

class QStackedWidget;

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void attemptLogin();
    void advanceSignUpStep();
    void attemptRegistration();
    void handleLoginSucceeded();
    void handleLoginFailed(const QString &message);
    void handleRegistrationSucceeded(const QString &userId);
    void handleRegistrationFailed(const QString &message);
    void attemptPasswordReset();
    void handlePasswordResetSucceeded(const QString &userId);
    void handlePasswordResetFailed(const QString &message);
    void openSignUpDialog();
    void showLoginPage();
    void toggleTheme();
    void togglePasswordVisibility();
    void showForgotPasswordHelp();

private:
    QWidget *createFieldShell(QWidget *owner, const QString &tag, QLineEdit *edit, QWidget *trailingWidget = nullptr);
    void applyTheme();
    void applyCurrentPageText();
    void syncFormWidths();
    void recalculateDialogSize(bool preserveCurrentHeight = false);
    void positionDialog();
    void beginWindowDrag(const QPoint &globalPos);
    bool startSystemWindowMove();
    void updateWindowDrag(const QPoint &globalPos);
    void endWindowDrag();
    void setBusy(bool busy);
    void updateStatusMessage(const QString &message, bool isError = false);
    QString normalizedBaseUrl(bool *ok = nullptr) const;
    QString normalizedServerHost(bool *ok = nullptr) const;
    bool isSignUpPageActive() const;
    bool isSignUpStepTwoActive() const;
    bool isPasswordResetPageActive() const;
    void resetSignUpStepTwoInputs();
    void resetPasswordResetInputs();
    void updateServerFieldVisibility();

private:
    bool m_isDark = true;
    bool m_busy = false;
    bool m_draggingWindow = false;
    QPoint m_dragOffset;

    QWidget *m_root = nullptr;
    QFrame *m_card = nullptr;
    QWidget *m_cardAccent = nullptr;
    QStackedWidget *m_formStack = nullptr;
    QWidget *m_loginPage = nullptr;
    QWidget *m_signUpPage = nullptr;
    QWidget *m_passwordResetPage = nullptr;
    QStackedWidget *m_signUpStepStack = nullptr;
    QWidget *m_signUpStepOnePage = nullptr;
    QWidget *m_signUpStepTwoPage = nullptr;

    QLabel *m_cardTitleLabel = nullptr;
    QLabel *m_cardSubtitleLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_healthDot = nullptr;
    QLabel *m_healthLabel = nullptr;

    QWidget *m_serverFieldContainer = nullptr;
    QLineEdit *m_serverUrlEdit = nullptr;
    QLineEdit *m_userIdEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLineEdit *m_signUpUserIdEdit = nullptr;
    QLineEdit *m_signUpNameEdit = nullptr;
    QLineEdit *m_signUpEmailEdit = nullptr;
    QLineEdit *m_signUpPasswordEdit = nullptr;
    QLineEdit *m_signUpConfirmPasswordEdit = nullptr;
    QLineEdit *m_resetUserIdEdit = nullptr;
    QLineEdit *m_resetNameEdit = nullptr;
    QLineEdit *m_resetEmailEdit = nullptr;
    QLineEdit *m_resetPasswordEdit = nullptr;
    QLineEdit *m_resetConfirmPasswordEdit = nullptr;

    QCheckBox *m_rememberCheck = nullptr;

    QPushButton *m_passwordToggleButton = nullptr;
    QPushButton *m_submitButton = nullptr;
    QPushButton *m_signUpButton = nullptr;
    QPushButton *m_signUpNextButton = nullptr;
    QPushButton *m_signUpPrevButton = nullptr;
    QPushButton *m_registerButton = nullptr;
    QPushButton *m_backButton = nullptr;
    QPushButton *m_resetButton = nullptr;
    QPushButton *m_resetBackButton = nullptr;
    QPushButton *m_helpButton = nullptr;
    QPushButton *m_themeButton = nullptr;
    QPushButton *m_closeButton = nullptr;
};

#endif // LOGINDIALOG_H
