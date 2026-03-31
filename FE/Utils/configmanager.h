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
        if(getRobotIp().isEmpty()) setRobotIp("ws://192.168.0.237:9090");
        // getUseCustomCCTV defaults to false if not set
        // getUseRtsps defaults to false if not set
        // getUseCustomCCTV defaults to false if not set, handled by QSettings default value
    }

    // Getter
    QString getCameraIp() const { return m_settings->value("Network/CameraIP", "192.168.0.39").toString(); }
    QString getCameraPort() const { return m_settings->value("Network/CameraPort", "8554").toString(); }
    QString getRobotIp() const { return m_settings->value("Network/RobotIP", "ws://192.168.0.237:9090").toString(); }
    bool getUseCustomCCTV() const { return m_settings->value("Network/UseCustomCCTV", false).toBool(); }
    bool getUseRtsps() const { return m_settings->value("Network/UseRtsps", false).toBool(); } // [New]
    bool getManualControl() const { return m_settings->value("Control/ManualControl", false).toBool(); }

    // Setter (저장)
    void setCameraIp(const QString &ip) {
        m_settings->setValue("Network/CameraIP", ip);
        emit configChanged();
    }
    void setCameraPort(const QString &port) {
        m_settings->setValue("Network/CameraPort", port);
        emit configChanged();
    }
    void setRobotIp(const QString &ip) {
        m_settings->setValue("Network/RobotIP", ip);
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
