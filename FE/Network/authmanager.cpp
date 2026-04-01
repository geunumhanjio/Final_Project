#include "authmanager.h"

#include <climits>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMutexLocker>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <gst/rtsp/gstrtspmessage.h>

namespace {

constexpr int kRefreshLeewaySeconds = 60;
constexpr int kRefreshRetryDelayMs = 15000;
constexpr int kRequestTimeoutMs = 5000;
constexpr const char *kRtspAuthHookKey = "veda.rtsp.auth.hook";

QString endpointSummary(const QUrl &url)
{
    const int port = (url.port() > 0) ? url.port() : (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0 ? 443 : 80);
    return QStringLiteral("%1://%2:%3").arg(url.scheme(), url.host()).arg(port);
}

QString describeNetworkFailure(QNetworkReply *reply, const QUrl &url, const QString &apiMessage)
{
    if (!apiMessage.isEmpty()) {
        return apiMessage;
    }

    const QString endpoint = endpointSummary(url);
    const int port = (url.port() > 0) ? url.port() : (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0 ? 443 : 80);

    switch (reply->error()) {
    case QNetworkReply::ConnectionRefusedError:
        return QStringLiteral("Cannot reach login server at %1. TCP connection to %2:%3 was refused. Check that the server is running and the port is open.")
            .arg(endpoint, url.host()).arg(port);
    case QNetworkReply::HostNotFoundError:
        return QStringLiteral("Login server host %1 could not be resolved. Check the IP or DNS name.").arg(url.host());
    case QNetworkReply::TimeoutError:
        return QStringLiteral("Login request to %1 timed out after %2 seconds. Check network routing and server health.")
            .arg(endpoint).arg(kRequestTimeoutMs / 1000);
    case QNetworkReply::RemoteHostClosedError:
        return QStringLiteral("Login server at %1 closed the connection before replying.").arg(endpoint);
    default:
        break;
    }

    return QStringLiteral("Request to %1 failed: %2").arg(endpoint, reply->errorString());
}

gboolean beforeSendCallback(GstElement *source, GstRTSPMessage *message, gpointer userData)
{
    Q_UNUSED(source);
    Q_UNUSED(userData);

    const QString headerValue = AuthManager::instance().authorizationHeaderValue();
    if (headerValue.isEmpty()) {
        return TRUE;
    }

    const QByteArray headerUtf8 = headerValue.toUtf8();
    gst_rtsp_message_add_header(message, GST_RTSP_HDR_AUTHORIZATION, headerUtf8.constData());
    return TRUE;
}

bool matchesResetIdentity(const QJsonObject &object,
                          const QString &userId,
                          const QString &name,
                          const QString &email)
{
    const QString responseUserId = object.value(QStringLiteral("id")).toString().trimmed();
    const QString responseName = object.value(QStringLiteral("name")).toString().trimmed();
    const QString responseEmail = object.value(QStringLiteral("email")).toString().trimmed();

    return responseUserId.compare(userId.trimmed(), Qt::CaseInsensitive) == 0
        && responseName.compare(name.trimmed(), Qt::CaseInsensitive) == 0
        && responseEmail.compare(email.trimmed(), Qt::CaseInsensitive) == 0;
}

} // namespace

bool AuthManager::Session::hasAccessToken(int leewaySeconds) const
{
    if (accessToken.trimmed().isEmpty()) {
        return false;
    }

    if (accessExpiresAt <= 0) {
        return true;
    }

    return (QDateTime::currentSecsSinceEpoch() + leewaySeconds) < accessExpiresAt;
}

bool AuthManager::Session::hasRefreshToken(int leewaySeconds) const
{
    if (refreshToken.trimmed().isEmpty()) {
        return false;
    }

    if (refreshExpiresAt <= 0) {
        return true;
    }

    return (QDateTime::currentSecsSinceEpoch() + leewaySeconds) < refreshExpiresAt;
}

void AuthManager::Session::clear()
{
    userId.clear();
    accessToken.clear();
    refreshToken.clear();
    tokenType = QStringLiteral("Bearer");
    accessExpiresAt = 0;
    refreshExpiresAt = 0;
}

AuthManager::AuthManager(QObject *parent)
    : QObject(parent)
{
    m_refreshTimer.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout, this, &AuthManager::refreshAccessToken);
}

void AuthManager::login(const QString &baseUrl, const QString &userId, const QString &password)
{
    const QString normalizedBaseUrl = normalizeBaseUrl(baseUrl);
    if (normalizedBaseUrl.isEmpty() || userId.trimmed().isEmpty() || password.isEmpty()) {
        emit loginFailed(QStringLiteral("Server URL, operator ID, and password are required."));
        return;
    }

    if (m_loginReply) {
        m_loginReply->abort();
        m_loginReply->deleteLater();
        m_loginReply.clear();
    }

    QUrl url(normalizedBaseUrl + QStringLiteral("/login"));
    if (!url.isValid()) {
        emit loginFailed(QStringLiteral("Login server URL is invalid."));
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(kRequestTimeoutMs);

    const QJsonObject body{
        { QStringLiteral("id"), userId.trimmed() },
        { QStringLiteral("password"), password }
    };

    emit loginStarted();
    QNetworkReply *reply = m_networkAccessManager.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_loginReply = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply, normalizedBaseUrl, userId, url]() {
        const QByteArray payload = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool success = (reply->error() == QNetworkReply::NoError) && (statusCode >= 200 && statusCode < 300);

        if (m_loginReply == reply) {
            m_loginReply.clear();
        }

        const QString apiMessage = extractErrorMessage(payload, QString());
        if (success) {
            QString errorMessage;
            if (applyLoginPayload(normalizedBaseUrl, userId.trimmed(), payload, &errorMessage)) {
                m_authenticationRequiredEmitted = false;
                emit loginSucceeded();
                emit sessionChanged(true);
            } else {
                emit loginFailed(errorMessage.isEmpty()
                                     ? QStringLiteral("Login response parsing failed.")
                                     : errorMessage);
            }
        } else {
            emit loginFailed(describeNetworkFailure(reply, url, apiMessage));
        }

        reply->deleteLater();
    });
}

void AuthManager::registerUser(const QString &baseUrl,
                               const QString &userId,
                               const QString &name,
                               const QString &email,
                               const QString &password)
{
    const QString normalizedBaseUrl = normalizeBaseUrl(baseUrl);
    if (normalizedBaseUrl.isEmpty()
        || userId.trimmed().isEmpty()
        || name.trimmed().isEmpty()
        || email.trimmed().isEmpty()
        || password.isEmpty()) {
        emit registrationFailed(QStringLiteral("Server URL, operator ID, name, email, and password are required."));
        return;
    }

    if (m_registrationReply) {
        m_registrationReply->abort();
        m_registrationReply->deleteLater();
        m_registrationReply.clear();
    }

    QUrl url(normalizedBaseUrl + QStringLiteral("/users"));
    if (!url.isValid()) {
        emit registrationFailed(QStringLiteral("Registration server URL is invalid."));
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(kRequestTimeoutMs);

    const QJsonObject body{
        { QStringLiteral("id"), userId.trimmed() },
        { QStringLiteral("name"), name.trimmed() },
        { QStringLiteral("email"), email.trimmed() },
        { QStringLiteral("password"), password }
    };

    emit registrationStarted();
    QNetworkReply *reply = m_networkAccessManager.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_registrationReply = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply, url, userId]() {
        const QByteArray payload = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool success = (reply->error() == QNetworkReply::NoError) && (statusCode >= 200 && statusCode < 300);

        if (m_registrationReply == reply) {
            m_registrationReply.clear();
        }

        const QString apiMessage = extractErrorMessage(payload, QString());
        if (success) {
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                emit registrationFailed(QStringLiteral("Registration response is not valid JSON."));
            } else {
                const QString createdUserId = document.object().value(QStringLiteral("id")).toString().trimmed();
                emit registrationSucceeded(createdUserId.isEmpty() ? userId.trimmed() : createdUserId);
            }
        } else {
            emit registrationFailed(describeNetworkFailure(reply, url, apiMessage));
        }

        reply->deleteLater();
    });
}

void AuthManager::resetPassword(const QString &baseUrl,
                                const QString &userId,
                                const QString &name,
                                const QString &email,
                                const QString &newPassword)
{
    const QString normalizedBaseUrl = normalizeBaseUrl(baseUrl);
    const QString trimmedUserId = userId.trimmed();
    const QString trimmedName = name.trimmed();
    const QString trimmedEmail = email.trimmed();

    if (normalizedBaseUrl.isEmpty()
        || trimmedUserId.isEmpty()
        || trimmedName.isEmpty()
        || trimmedEmail.isEmpty()
        || newPassword.isEmpty()) {
        emit passwordResetFailed(QStringLiteral("Server URL, operator ID, name, email, and new password are required."));
        return;
    }

    if (m_passwordResetLookupReply) {
        m_passwordResetLookupReply->abort();
        m_passwordResetLookupReply->deleteLater();
        m_passwordResetLookupReply.clear();
    }

    if (m_passwordResetUpdateReply) {
        m_passwordResetUpdateReply->abort();
        m_passwordResetUpdateReply->deleteLater();
        m_passwordResetUpdateReply.clear();
    }

    const QString encodedUserId = QString::fromUtf8(QUrl::toPercentEncoding(trimmedUserId));
    QUrl lookupUrl(normalizedBaseUrl + QStringLiteral("/users/") + encodedUserId);
    if (!lookupUrl.isValid()) {
        emit passwordResetFailed(QStringLiteral("Password reset lookup URL is invalid."));
        return;
    }

    QNetworkRequest lookupRequest(lookupUrl);
    lookupRequest.setTransferTimeout(kRequestTimeoutMs);

    emit passwordResetStarted();
    QNetworkReply *lookupReply = m_networkAccessManager.get(lookupRequest);
    m_passwordResetLookupReply = lookupReply;

    connect(lookupReply, &QNetworkReply::finished, this, [this,
                                                          lookupReply,
                                                          lookupUrl,
                                                          normalizedBaseUrl,
                                                          trimmedUserId,
                                                          trimmedName,
                                                          trimmedEmail,
                                                          newPassword,
                                                          encodedUserId]() {
        const QByteArray payload = lookupReply->readAll();
        const int statusCode = lookupReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool success = (lookupReply->error() == QNetworkReply::NoError) && (statusCode >= 200 && statusCode < 300);

        if (m_passwordResetLookupReply == lookupReply) {
            m_passwordResetLookupReply.clear();
        }

        const QString apiMessage = extractErrorMessage(payload, QString());
        if (!success) {
            emit passwordResetFailed(describeNetworkFailure(lookupReply, lookupUrl, apiMessage));
            lookupReply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            emit passwordResetFailed(QStringLiteral("User lookup response is not valid JSON."));
            lookupReply->deleteLater();
            return;
        }

        if (!matchesResetIdentity(document.object(), trimmedUserId, trimmedName, trimmedEmail)) {
            emit passwordResetFailed(QStringLiteral("The provided ID, name, and email do not match this account."));
            lookupReply->deleteLater();
            return;
        }

        QUrl updateUrl(normalizedBaseUrl + QStringLiteral("/users/") + encodedUserId);
        if (!updateUrl.isValid()) {
            emit passwordResetFailed(QStringLiteral("Password update URL is invalid."));
            lookupReply->deleteLater();
            return;
        }

        QNetworkRequest updateRequest(updateUrl);
        updateRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        updateRequest.setTransferTimeout(kRequestTimeoutMs);

        const QJsonObject body{
            { QStringLiteral("password"), newPassword }
        };

        QNetworkReply *updateReply = m_networkAccessManager.put(updateRequest, QJsonDocument(body).toJson(QJsonDocument::Compact));
        m_passwordResetUpdateReply = updateReply;

        connect(updateReply, &QNetworkReply::finished, this, [this, updateReply, updateUrl, trimmedUserId]() {
            const QByteArray updatePayload = updateReply->readAll();
            const int updateStatusCode = updateReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const bool updateSuccess = (updateReply->error() == QNetworkReply::NoError)
                && (updateStatusCode >= 200 && updateStatusCode < 300);

            if (m_passwordResetUpdateReply == updateReply) {
                m_passwordResetUpdateReply.clear();
            }

            const QString updateApiMessage = extractErrorMessage(updatePayload, QString());
            if (updateSuccess) {
                emit passwordResetSucceeded(trimmedUserId);
            } else {
                emit passwordResetFailed(describeNetworkFailure(updateReply, updateUrl, updateApiMessage));
            }

            updateReply->deleteLater();
        });

        lookupReply->deleteLater();
    });
}

void AuthManager::fetchCurrentUserProfile()
{
    QString baseUrlCopy;
    QString userIdCopy;
    {
        QMutexLocker locker(&m_mutex);
        baseUrlCopy = m_baseUrl;
        userIdCopy = m_session.userId.trimmed();
    }

    if (baseUrlCopy.isEmpty() || userIdCopy.isEmpty()) {
        emit userProfileFailed(QStringLiteral("User profile lookup requires an authenticated user."));
        return;
    }

    if (m_userProfileReply) {
        m_userProfileReply->abort();
        m_userProfileReply->deleteLater();
        m_userProfileReply.clear();
    }

    if (m_passwordResetLookupReply) {
        m_passwordResetLookupReply->abort();
        m_passwordResetLookupReply->deleteLater();
        m_passwordResetLookupReply.clear();
    }

    if (m_passwordResetUpdateReply) {
        m_passwordResetUpdateReply->abort();
        m_passwordResetUpdateReply->deleteLater();
        m_passwordResetUpdateReply.clear();
    }

    const QString encodedUserId = QString::fromUtf8(QUrl::toPercentEncoding(userIdCopy));
    QUrl url(baseUrlCopy + QStringLiteral("/users/") + encodedUserId);
    if (!url.isValid()) {
        emit userProfileFailed(QStringLiteral("User profile URL is invalid."));
        return;
    }

    QNetworkRequest request(url);
    request.setTransferTimeout(kRequestTimeoutMs);

    QNetworkReply *reply = m_networkAccessManager.get(request);
    m_userProfileReply = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply, url, userIdCopy]() {
        const QByteArray payload = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool success = (reply->error() == QNetworkReply::NoError) && (statusCode >= 200 && statusCode < 300);

        if (m_userProfileReply == reply) {
            m_userProfileReply.clear();
        }

        if (success) {
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                emit userProfileFailed(QStringLiteral("User profile response is not valid JSON."));
            } else {
                const QString email = document.object().value(QStringLiteral("email")).toString().trimmed();
                emit userProfileResolved(userIdCopy, email);
            }
        } else {
            const QString apiMessage = extractErrorMessage(payload, QString());
            emit userProfileFailed(describeNetworkFailure(reply, url, apiMessage));
        }

        reply->deleteLater();
    });
}

void AuthManager::refreshAccessToken()
{
    QString baseUrlCopy;
    Session sessionCopy;
    {
        QMutexLocker locker(&m_mutex);
        baseUrlCopy = m_baseUrl;
        sessionCopy = m_session;
    }

    if (m_refreshReply || baseUrlCopy.isEmpty()) {
        return;
    }

    if (!sessionCopy.hasRefreshToken()) {
        handleAuthenticationFailure(QStringLiteral("Refresh token has expired. Please sign in again."));
        return;
    }

    QUrl url(baseUrlCopy + QStringLiteral("/refresh"));
    if (!url.isValid()) {
        emit refreshFailed(QStringLiteral("Refresh server URL is invalid."));
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(kRequestTimeoutMs);

    const QJsonObject body{
        { QStringLiteral("refresh_token"), sessionCopy.refreshToken }
    };

    QNetworkReply *reply = m_networkAccessManager.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_refreshReply = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        const QByteArray payload = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool success = (reply->error() == QNetworkReply::NoError) && (statusCode >= 200 && statusCode < 300);

        if (m_refreshReply == reply) {
            m_refreshReply.clear();
        }

        const QString apiMessage = extractErrorMessage(payload, QString());
        if (success) {
            QString errorMessage;
            if (applyRefreshPayload(payload, &errorMessage)) {
                emit refreshSucceeded();
            } else {
                emit refreshFailed(errorMessage.isEmpty()
                                      ? QStringLiteral("Refresh response parsing failed.")
                                      : errorMessage);
            }
        } else {
            const QString message = describeNetworkFailure(reply, url, apiMessage);
            if (isAuthenticationStatus(statusCode)) {
                handleAuthenticationFailure(message);
            } else {
                emit refreshFailed(message);
                m_refreshTimer.start(kRefreshRetryDelayMs);
            }
        }

        reply->deleteLater();
    });
}

void AuthManager::clearSession()
{
    if (m_userProfileReply) {
        m_userProfileReply->abort();
        m_userProfileReply->deleteLater();
        m_userProfileReply.clear();
    }

    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        changed = !m_session.accessToken.isEmpty() || !m_session.refreshToken.isEmpty() || !m_session.userId.isEmpty();
        m_session.clear();
        m_baseUrl.clear();
    }

    m_refreshTimer.stop();
    if (changed) {
        emit sessionChanged(false);
    }
}

bool AuthManager::isAuthenticated() const
{
    QMutexLocker locker(&m_mutex);
    return m_session.hasAccessToken();
}

bool AuthManager::hasRefreshToken() const
{
    QMutexLocker locker(&m_mutex);
    return m_session.hasRefreshToken();
}

bool AuthManager::isAccessTokenExpiringSoon(int leewaySeconds) const
{
    QMutexLocker locker(&m_mutex);
    if (m_session.accessToken.isEmpty()) {
        return true;
    }
    return !m_session.hasAccessToken(leewaySeconds);
}

QString AuthManager::baseUrl() const
{
    QMutexLocker locker(&m_mutex);
    return m_baseUrl;
}

QString AuthManager::currentUserId() const
{
    QMutexLocker locker(&m_mutex);
    return m_session.userId;
}

QString AuthManager::accessToken() const
{
    QMutexLocker locker(&m_mutex);
    return m_session.accessToken;
}

QString AuthManager::authorizationHeaderValue() const
{
    QMutexLocker locker(&m_mutex);
    if (m_session.accessToken.isEmpty()) {
        return QString();
    }

    const QString tokenType = m_session.tokenType.trimmed().isEmpty()
                                  ? QStringLiteral("Bearer")
                                  : m_session.tokenType.trimmed();
    return QStringLiteral("%1 %2").arg(tokenType, m_session.accessToken);
}

QString AuthManager::authorizedRtspUrl(const QString &url) const
{
    const QString token = accessToken();
    if (token.isEmpty()) {
        return url;
    }

    QUrl rtspUrl(url);
    if (!rtspUrl.isValid()) {
        return url;
    }

    if (rtspUrl.scheme().compare(QStringLiteral("rtsps"), Qt::CaseInsensitive) != 0) {
        return url;
    }

    QUrlQuery query(rtspUrl);
    query.removeAllQueryItems(QStringLiteral("token"));
    query.addQueryItem(QStringLiteral("token"), token);
    rtspUrl.setQuery(query);
    return rtspUrl.toString(QUrl::FullyEncoded);
}

bool AuthManager::configureRtspSource(GstElement *source)
{
    if (!source || accessToken().isEmpty()) {
        return false;
    }

    if (g_signal_lookup("before-send", G_OBJECT_TYPE(source)) == 0) {
        return false;
    }

    if (g_object_get_data(G_OBJECT(source), kRtspAuthHookKey)) {
        return true;
    }

    g_signal_connect(source, "before-send", G_CALLBACK(beforeSendCallback), nullptr);
    g_object_set_data(G_OBJECT(source), kRtspAuthHookKey, GINT_TO_POINTER(1));
    return true;
}

bool AuthManager::applyLoginPayload(const QString &baseUrl, const QString &userId, const QByteArray &payload, QString *errorMessage)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Login response is not valid JSON.");
        }
        return false;
    }

    const QJsonObject object = document.object();
    Session session;
    session.userId = userId;
    session.accessToken = object.value(QStringLiteral("access_token")).toString().trimmed();
    session.refreshToken = object.value(QStringLiteral("refresh_token")).toString().trimmed();
    session.tokenType = object.value(QStringLiteral("token_type")).toString().trimmed();
    if (session.tokenType.isEmpty()) {
        session.tokenType = QStringLiteral("Bearer");
    }
    session.accessExpiresAt = object.value(QStringLiteral("access_expires_at")).toVariant().toLongLong();
    session.refreshExpiresAt = object.value(QStringLiteral("refresh_expires_at")).toVariant().toLongLong();

    if (session.accessToken.isEmpty() || session.refreshToken.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Login response did not include access and refresh tokens.");
        }
        return false;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_baseUrl = baseUrl;
        m_session = session;
    }

    scheduleRefresh();
    return true;
}

bool AuthManager::applyRefreshPayload(const QByteArray &payload, QString *errorMessage)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Refresh response is not valid JSON.");
        }
        return false;
    }

    const QJsonObject object = document.object();
    const QString accessTokenValue = object.value(QStringLiteral("access_token")).toString().trimmed();
    if (accessTokenValue.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Refresh response did not include a new access token.");
        }
        return false;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_session.accessToken = accessTokenValue;
        const QString tokenTypeValue = object.value(QStringLiteral("token_type")).toString().trimmed();
        if (!tokenTypeValue.isEmpty()) {
            m_session.tokenType = tokenTypeValue;
        }
        const qint64 accessExpiresAtValue = object.value(QStringLiteral("access_expires_at")).toVariant().toLongLong();
        if (accessExpiresAtValue > 0) {
            m_session.accessExpiresAt = accessExpiresAtValue;
        }
    }

    scheduleRefresh();
    return true;
}

void AuthManager::scheduleRefresh()
{
    qint64 msUntilRefresh = 0;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_session.hasRefreshToken()) {
            m_refreshTimer.stop();
            return;
        }

        if (m_session.accessExpiresAt <= 0) {
            m_refreshTimer.stop();
            return;
        }

        const qint64 currentMs = QDateTime::currentMSecsSinceEpoch();
        const qint64 refreshAtMs = qMax<qint64>(0, (m_session.accessExpiresAt - kRefreshLeewaySeconds) * 1000);
        msUntilRefresh = qMax<qint64>(0, refreshAtMs - currentMs);
    }

    const int safeDelay = (msUntilRefresh > INT_MAX) ? INT_MAX : static_cast<int>(msUntilRefresh);
    m_refreshTimer.start(safeDelay);
}

void AuthManager::handleAuthenticationFailure(const QString &message)
{
    clearSession();
    if (m_authenticationRequiredEmitted) {
        return;
    }

    m_authenticationRequiredEmitted = true;
    emit authenticationRequired(message.isEmpty()
                                    ? QStringLiteral("Authentication has expired. Please sign in again.")
                                    : message);
}

QString AuthManager::normalizeBaseUrl(const QString &baseUrl) const
{
    QString normalized = baseUrl.trimmed();
    if (normalized.isEmpty()) {
        return normalized;
    }

    if (!normalized.contains(QStringLiteral("://"))) {
        normalized.prepend(QStringLiteral("http://"));
    }

    QUrl url(normalized);
    if (!url.isValid() || url.host().isEmpty()) {
        while (normalized.endsWith('/')) {
            normalized.chop(1);
        }
        return normalized;
    }

    if (url.port(-1) < 0) {
        url.setPort(8080);
    }

    QString path = url.path();
    while (path.endsWith('/')) {
        path.chop(1);
    }
    if (path == QStringLiteral("/")) {
        path.clear();
    }
    url.setPath(path);

    return url.toString(QUrl::RemoveFragment | QUrl::RemoveQuery);
}

QString AuthManager::extractErrorMessage(const QByteArray &payload, const QString &fallback) const
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
        const QJsonObject object = document.object();
        const QString message = object.value(QStringLiteral("message")).toString().trimmed();
        const QString error = object.value(QStringLiteral("error")).toString().trimmed();
        if (!message.isEmpty() && !error.isEmpty()) {
            return QStringLiteral("%1 (%2)").arg(message, error);
        }
        if (!message.isEmpty()) {
            return message;
        }
        if (!error.isEmpty()) {
            return error;
        }
    }

    return fallback;
}

bool AuthManager::isAuthenticationStatus(int statusCode) const
{
    return statusCode == 401 || statusCode == 403;
}
