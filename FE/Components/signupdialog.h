#ifndef SIGNUPDIALOG_H
#define SIGNUPDIALOG_H

#include <QDialog>
#include <QShowEvent>

class QLabel;
class QLineEdit;
class QPushButton;
class QFrame;

class SignUpDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SignUpDialog(const QString &baseUrl, bool darkTheme, QWidget *parent = nullptr);

    QString registeredUserId() const { return m_registeredUserId; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void attemptRegistration();
    void handleRegistrationSucceeded(const QString &userId);
    void handleRegistrationFailed(const QString &message);

private:
    QWidget *createFieldShell(const QString &tag, QLineEdit *edit, QWidget *trailingWidget = nullptr);
    void applyTheme();
    void positionDialog();
    void setBusy(bool busy);
    void updateStatusMessage(const QString &message, bool isError = false);

private:
    QString m_baseUrl;
    QString m_registeredUserId;
    bool m_isDark = true;
    bool m_busy = false;

    QFrame *m_card = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_subtitleLabel = nullptr;
    QLabel *m_gatewayLabel = nullptr;
    QLabel *m_statusLabel = nullptr;

    QLineEdit *m_userIdEdit = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_emailEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLineEdit *m_confirmPasswordEdit = nullptr;

    QPushButton *m_passwordToggleButton = nullptr;
    QPushButton *m_confirmPasswordToggleButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_submitButton = nullptr;
};

#endif // SIGNUPDIALOG_H
