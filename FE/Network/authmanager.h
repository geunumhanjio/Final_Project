#ifndef AUTHMANAGER_H
#define AUTHMANAGER_H

#include <QObject>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <gst/gst.h>

class AuthManager : public QObject
{
    Q_OBJECT

public:
    static AuthManager& instance()
    {
        static AuthManager _instance;
        return _instance;
    }

    void login(const QString &baseUrl, const QString &userId, const QString &password);
    void registerUser(const QString &baseUrl,
                      const QString &userId,
                      const QString &name,
                      const QString &email,
                      const QString &password);
    void resetPassword(const QString &baseUrl,
                       const QString &userId,
                       const QString &name,
                       const QString &email,
                       const QString &newPassword);
    void fetchCurrentUserProfile();
    void refreshAccessToken();
    void clearSession();

    bool isAuthenticated() const;
    bool hasRefreshToken() const;
    bool isAccessTokenExpiringSoon(int leewaySeconds = 60) const;

    QString baseUrl() const;
    QString currentUserId() const;
    QString accessToken() const;
    QString authorizationHeaderValue() const;
    QString authorizedRtspUrl(const QString &url) const;
    bool configureRtspSource(GstElement *source);

signals:
    void loginStarted();
    void loginSucceeded();
    void loginFailed(const QString &message);
    void registrationStarted();
    void registrationSucceeded(const QString &userId);
    void registrationFailed(const QString &message);
    void passwordResetStarted();
    void passwordResetSucceeded(const QString &userId);
    void passwordResetFailed(const QString &message);
    void refreshSucceeded();
    void refreshFailed(const QString &message);
    void userProfileResolved(const QString &userId, const QString &email);
    void userProfileFailed(const QString &message);
    void authenticationRequired(const QString &message);
    void sessionChanged(bool authenticated);

private:
    struct Session {
        QString userId;
        QString accessToken;
        QString refreshToken;
        QString tokenType = QStringLiteral("Bearer");
        qint64 accessExpiresAt = 0;
        qint64 refreshExpiresAt = 0;

        bool hasAccessToken(int leewaySeconds = 0) const;
        bool hasRefreshToken(int leewaySeconds = 0) const;
        void clear();
    };

    explicit AuthManager(QObject *parent = nullptr);
    AuthManager(const AuthManager&) = delete;
    AuthManager& operator=(const AuthManager&) = delete;

    bool applyLoginPayload(const QString &baseUrl, const QString &userId, const QByteArray &payload, QString *errorMessage = nullptr);
    bool applyRefreshPayload(const QByteArray &payload, QString *errorMessage = nullptr);
    void scheduleRefresh();
    void handleAuthenticationFailure(const QString &message);
    QString normalizeBaseUrl(const QString &baseUrl) const;
    QString extractErrorMessage(const QByteArray &payload, const QString &fallback) const;
    bool isAuthenticationStatus(int statusCode) const;

private:
    mutable QMutex m_mutex;
    QNetworkAccessManager m_networkAccessManager;
    QPointer<QNetworkReply> m_loginReply;
    QPointer<QNetworkReply> m_registrationReply;
    QPointer<QNetworkReply> m_passwordResetLookupReply;
    QPointer<QNetworkReply> m_passwordResetUpdateReply;
    QPointer<QNetworkReply> m_refreshReply;
    QPointer<QNetworkReply> m_userProfileReply;
    QTimer m_refreshTimer;
    Session m_session;
    QString m_baseUrl;
    bool m_authenticationRequiredEmitted = false;
};

#endif // AUTHMANAGER_H
