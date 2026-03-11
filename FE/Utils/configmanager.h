#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QSettings>
#include <QDir>
#include <QCoreApplication>
#include <QUrl>

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
        // getUseCustomCCTV defaults to false if not set
        // getUseRtsps defaults to false if not set
    }

    // Getter
    QString getCameraIp() const { return m_settings->value("Network/CameraIP", "192.168.0.39").toString(); }
    QString getCameraPort() const { return m_settings->value("Network/CameraPort", "8554").toString(); }
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

    explicit ConfigManager(QObject *parent = nullptr) : QObject(parent) {
        // 실행 파일 위치에 settings.ini 파일 생성
        QString path = QCoreApplication::applicationDirPath() + "/settings.ini";
        m_settings = new QSettings(path, QSettings::IniFormat, this);
    }
    QSettings *m_settings;
};

#endif // CONFIGMANAGER_H
