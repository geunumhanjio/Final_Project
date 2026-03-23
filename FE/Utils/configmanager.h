#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QSettings>
#include <QDir>
#include <QCoreApplication>
#include <QUrl>
#include <cmath>

class ConfigManager : public QObject
{
    Q_OBJECT
public:
    static ConfigManager& instance() {
        static ConfigManager _instance;
        return _instance;
    }

    // 기본값 설정
    void loadDefaults() {
        if(getCameraIp().isEmpty()) setCameraIp("192.168.0.39");
        if(getCameraPort().isEmpty()) setCameraPort("8554");
        if(getRobotSettingValue().isEmpty()) setRobotIp("192.168.0.237");
        const QString loginServerUrl = getLoginServerUrl();
        if (loginServerUrl.isEmpty()
            || loginServerUrl == "127.0.0.1") {
            setLoginServerUrl("192.168.0.110");
        }
        if (!m_settings->contains("Network/CustomCCTVUsername")) setCustomCctvUsername("admin");
        if (!m_settings->contains("Network/CustomCCTVPassword")) setCustomCctvPassword("5hanwha!");
        if (!m_settings->contains("Control/LinearX")) setManualLinearX(0.30);
        if (!m_settings->contains("Control/AngularZ")) setManualAngularZ(0.50);
        if (!m_settings->contains("Navigation/AutoSpeed")) setAutoNavSpeed(0.15);
        if (!m_settings->contains("UI/DarkTheme")) setDarkTheme(true);
        if (!m_settings->contains("Auth/RememberUser")) setRememberUser(false);
        // getUseCustomCCTV defaults to false if not set
        // getUseRtsps defaults to false if not set
    }

    // Getter
    QString getCameraIp() const { return m_settings->value("Network/CameraIP", "192.168.0.39").toString(); }
    QString getCameraPort() const { return m_settings->value("Network/CameraPort", "8554").toString(); }
    QString getCustomCctvUsername() const {
        const QString userName = m_settings->value("Network/CustomCCTVUsername", "admin").toString().trimmed();
        return userName.isEmpty() ? QStringLiteral("admin") : userName;
    }
    QString getCustomCctvPassword() const {
        const QString password = m_settings->value("Network/CustomCCTVPassword", "5hanwha!").toString();
        return password.isEmpty() ? QStringLiteral("5hanwha!") : password;
    }
    QString getRobotIp() const { return getRobotWsUrl(); }
    QString getRobotHost() const {
        const QString host = extractRobotHost(getRobotSettingValue());
        return host.isEmpty() ? QStringLiteral("192.168.0.237") : host;
    }
    QString getRobotWsUrl() const {
        return QString("ws://%1:9090").arg(getRobotHost());
    }
    QString getRobotRtspUrl() const {
        return QString("rtsp://%1:9554/camera").arg(getRobotHost());
    }
    QString getRobotRtspIspUrl() const {
        return QString("rtsp://%1:9554/camera_isp").arg(getRobotHost());
    }
    QString getLoginServerUrl() const {
        return normalizeLoginServerInput(m_settings->value("Auth/LoginServerUrl", "192.168.0.110").toString());
    }
    QString getActiveUserId() const {
        return m_settings->value("Auth/ActiveUserId").toString().trimmed();
    }
    QString getActiveUserEmail() const {
        return m_settings->value("Auth/ActiveUserEmail").toString().trimmed();
    }
    QString getActiveAuthMode() const {
        return m_settings->value("Auth/ActiveAuthMode").toString().trimmed();
    }
    bool getRememberUser() const { return m_settings->value("Auth/RememberUser", false).toBool(); }
    QString getRememberedUserId() const {
        return getRememberUser() ? m_settings->value("Auth/RememberedUserId").toString().trimmed() : QString();
    }
    bool getDarkTheme() const { return m_settings->value("UI/DarkTheme", true).toBool(); }
    bool getUseCustomCCTV() const { return m_settings->value("Network/UseCustomCCTV", false).toBool(); }
    bool getUseRtsps() const { return m_settings->value("Network/UseRtsps", false).toBool(); } // [New]
    bool getManualControl() const { return m_settings->value("Control/ManualControl", false).toBool(); }
    double getManualLinearX() const { return clampManualValue(m_settings->value("Control/LinearX", 0.30).toDouble(), 0.30); }
    double getManualAngularZ() const { return clampManualValue(m_settings->value("Control/AngularZ", 0.50).toDouble(), 0.50); }
    double getAutoNavSpeed() const { return clampManualValue(m_settings->value("Navigation/AutoSpeed", 0.15).toDouble(), 0.15); }

    // Setter (저장)
    void setCameraIp(const QString &ip) {
        m_settings->setValue("Network/CameraIP", ip);
        emit configChanged();
    }
    void setCameraPort(const QString &port) {
        m_settings->setValue("Network/CameraPort", port);
        emit configChanged();
    }
    void setCustomCctvUsername(const QString &userName) {
        m_settings->setValue("Network/CustomCCTVUsername", userName.trimmed());
        emit configChanged();
    }
    void setCustomCctvPassword(const QString &password) {
        m_settings->setValue("Network/CustomCCTVPassword", password);
        emit configChanged();
    }
    void setRobotIp(const QString &ip) {
        m_settings->setValue("Network/RobotIP", ip);
        emit configChanged();
    }
    void setLoginServerUrl(const QString &url) {
        m_settings->setValue("Auth/LoginServerUrl", normalizeLoginServerInput(url));
        emit configChanged();
    }
    void setActiveUserId(const QString &userId) {
        m_settings->setValue("Auth/ActiveUserId", userId.trimmed());
        emit configChanged();
    }
    void setActiveUserEmail(const QString &email) {
        m_settings->setValue("Auth/ActiveUserEmail", email.trimmed());
        emit configChanged();
    }
    void setActiveAuthMode(const QString &authMode) {
        m_settings->setValue("Auth/ActiveAuthMode", authMode.trimmed());
        emit configChanged();
    }
    void clearActiveLogin() {
        m_settings->remove("Auth/ActiveUserId");
        m_settings->remove("Auth/ActiveUserEmail");
        m_settings->remove("Auth/ActiveAuthMode");
        emit configChanged();
    }
    void setRememberUser(bool remember) {
        m_settings->setValue("Auth/RememberUser", remember);
        if (!remember) {
            m_settings->remove("Auth/RememberedUserId");
        }
        emit configChanged();
    }
    void setRememberedUserId(const QString &id) {
        m_settings->setValue("Auth/RememberedUserId", id.trimmed());
        emit configChanged();
    }
    void setDarkTheme(bool dark) {
        m_settings->setValue("UI/DarkTheme", dark);
        emit configChanged();
    }
    void setUseCustomCCTV(bool use) {
        m_settings->setValue("Network/UseCustomCCTV", use);
        emit configChanged();
    }
    void setUseRtsps(bool use) { // [New]
        m_settings->setValue("Network/UseRtsps", use);
        emit configChanged();
    }
    void setManualControl(bool use) {
        m_settings->setValue("Control/ManualControl", use);
        emit configChanged();
    }
    void setManualLinearX(double value) {
        m_settings->setValue("Control/LinearX", normalizeManualValue(value));
        emit configChanged();
    }
    void setManualAngularZ(double value) {
        m_settings->setValue("Control/AngularZ", normalizeManualValue(value));
        emit configChanged();
    }
    void setAutoNavSpeed(double value) {
        m_settings->setValue("Navigation/AutoSpeed", normalizeManualValue(value));
        emit configChanged();
    }

signals:
    void configChanged(); // 설정이 바뀌면 다른 곳에 알림

private:
    static double clampManualValue(double value, double fallback) {
        if (std::isnan(value)) return fallback;
        if (value < 0.0) return 0.0;
        if (value > 1.0) return 1.0;
        return value;
    }

    static double normalizeManualValue(double value) {
        const double clamped = clampManualValue(value, 0.0);
        return std::round(clamped * 100.0) / 100.0;
    }

    QString getRobotSettingValue() const {
        return m_settings->value("Network/RobotIP", "192.168.0.237").toString().trimmed();
    }

    static QString extractRobotHost(QString value) {
        value = value.trimmed();
        if (value.isEmpty()) return QString();

        QUrl url(value);
        if (!url.host().isEmpty()) {
            return url.host();
        }

        if (!value.contains("://")) {
            url = QUrl("ws://" + value);
            if (!url.host().isEmpty()) {
                return url.host();
            }
        }

        if (value.startsWith("//")) {
            value.remove(0, 2);
        }

        const int slashIndex = value.indexOf('/');
        if (slashIndex >= 0) {
            value = value.left(slashIndex);
        }

        const int colonIndex = value.indexOf(':');
        if (colonIndex >= 0) {
            value = value.left(colonIndex);
        }

        return value.trimmed();
    }

    static QString normalizeLoginServerInput(QString value) {
        value = value.trimmed();
        if (value.isEmpty()) return QString();

        QUrl url(value);
        if (url.host().isEmpty() && !value.contains("://")) {
            url = QUrl(QStringLiteral("http://") + value);
        }

        QString host = url.host().trimmed();
        int port = url.port(-1);

        if (host.isEmpty()) {
            QString fallback = value;
            if (fallback.startsWith("//")) {
                fallback.remove(0, 2);
            }

            const int slashIndex = fallback.indexOf('/');
            if (slashIndex >= 0) {
                fallback = fallback.left(slashIndex);
            }

            host = fallback.trimmed();
            const int colonIndex = host.lastIndexOf(':');
            if (colonIndex > 0) {
                bool portOk = false;
                const int parsedPort = host.mid(colonIndex + 1).toInt(&portOk);
                if (portOk) {
                    port = parsedPort;
                    host = host.left(colonIndex).trimmed();
                }
            }
        }

        if (host.isEmpty()) {
            return QString();
        }

        if (port > 0 && port != 8080) {
            return QStringLiteral("%1:%2").arg(host, QString::number(port));
        }

        return host;
    }

    explicit ConfigManager(QObject *parent = nullptr) : QObject(parent) {
        // 실행 파일 위치에 settings.ini 파일 생성
        QString path = QCoreApplication::applicationDirPath() + "/settings.ini";
        m_settings = new QSettings(path, QSettings::IniFormat, this);
    }
    QSettings *m_settings;
};

#endif // CONFIGMANAGER_H
