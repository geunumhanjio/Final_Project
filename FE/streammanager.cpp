#include "streammanager.h"
#include "configmanager.h"
#include <QDebug>

StreamManager::StreamManager(QObject *parent) : QObject(parent)
{
    loadConfig();
}

void StreamManager::loadConfig()
{
    m_channels.clear();

    // ConfigManager에서 IP와 Port 가져오기
    ConfigManager::instance().loadDefaults(); // 초기값이 없으면 설정
    QString ip = ConfigManager::instance().getCameraIp();
    QString port = ConfigManager::instance().getCameraPort();
    bool useCustomCCTV = ConfigManager::instance().getUseCustomCCTV();

    qDebug() << "[StreamManager] Loading config with IP:" << ip << "Port:" << port << "UseCustomCCTV:" << useCustomCCTV;

    // 4 CCTV Channels
    for(int i = 0; i < 4; i++) {
        ChannelConfig config;
        config.id = i;
        config.name = QString("Camera %1").arg(i+1);
        
        // 가져온 변수 사용
        if (useCustomCCTV) {
            // Checked: rtsp://admin:5hanwha!@IP:PORT/ID/H.264/media.smp
            // ID is 0-based index
            QString url = QString("rtsp://admin:5hanwha!@%1:%2/%3/H.264/media.smp")
                              .arg(ip, port).arg(i);
            config.urlLow = url;
            config.urlHigh = url; 
        } else {
            // Unchecked: rtsp://IP:PORT/ch<ID+1>
            config.urlLow = QString("rtsp://%1:%2/ch%3").arg(ip, port).arg(i+1);
            config.urlHigh = QString("rtsp://%1:%2/ch%3_fhd").arg(ip, port).arg(i+1);
        }
        
        config.isActive = true;
        
        m_channels.append(config);
    }
    
    qDebug() << "[StreamManager] Loaded" << m_channels.size() << "channels.";
    emit configLoaded();
}

QString StreamManager::getLowQualityUrl(int index) const {
    if (index >= 0 && index < m_channels.size()) return m_channels[index].urlLow;
    return "";
}

QString StreamManager::getHighQualityUrl(int index) const {
    if (index >= 0 && index < m_channels.size()) return m_channels[index].urlHigh;
    return "";
}
