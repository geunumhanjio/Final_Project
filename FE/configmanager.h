#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QSettings>
#include <QDir>
#include <QCoreApplication>

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
        // getUseCustomCCTV defaults to false if not set, handled by QSettings default value
    }

    // Getter
    QString getCameraIp() const { return m_settings->value("Network/CameraIP", "192.168.0.39").toString(); }
    QString getCameraPort() const { return m_settings->value("Network/CameraPort", "8554").toString(); }
    bool getUseCustomCCTV() const { return m_settings->value("Network/UseCustomCCTV", false).toBool(); }

    // Setter (저장)
    void setCameraIp(const QString &ip) {
        m_settings->setValue("Network/CameraIP", ip);
        emit configChanged();
    }
    void setCameraPort(const QString &port) {
        m_settings->setValue("Network/CameraPort", port);
        emit configChanged();
    }
    void setUseCustomCCTV(bool use) {
        m_settings->setValue("Network/UseCustomCCTV", use);
        emit configChanged();
    }

signals:
    void configChanged(); // 설정이 바뀌면 다른 곳에 알림

private:
    explicit ConfigManager(QObject *parent = nullptr) : QObject(parent) {
        // 실행 파일 위치에 settings.ini 파일 생성
        QString path = QCoreApplication::applicationDirPath() + "/settings.ini";
        m_settings = new QSettings(path, QSettings::IniFormat, this);
    }
    QSettings *m_settings;
};

#endif // CONFIGMANAGER_H
