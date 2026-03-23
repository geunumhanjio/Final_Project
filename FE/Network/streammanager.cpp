#include "streammanager.h"
#include "configmanager.h"
#include <QDebug>
#include <QUrl>

namespace {

QString encodeUserInfo(const QString &value)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(value));
}

} // namespace

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
    const QString customCctvUser = ConfigManager::instance().getCustomCctvUsername();
    const QString customCctvPassword = ConfigManager::instance().getCustomCctvPassword();
    bool useRtsps = ConfigManager::instance().getUseRtsps(); // [New]

    // [New] If RTSPS mode, override scheme and port
    QString scheme = useRtsps ? "rtsps" : "rtsp";
    if (useRtsps) port = "8322"; // Guide recommends 8322 for RTSPS

    qDebug() << "[StreamManager] Loading config | Mode:" << scheme << "IP:" << ip << "Port:" << port << "UseCustomCCTV:" << useCustomCCTV;

    // 4 CCTV Channels
    for(int i = 0; i < 4; i++) {
        ChannelConfig config;
        config.id = i;
        config.name = QString("Camera %1").arg(i+1);
        
        // 가져온 변수 사용
        if (useCustomCCTV) {
            // Checked: rtsps?://<user>:<password>@IP:PORT/ID/H.264/media.smp
            const QString url = QString("%1://%2:%3@%4:%5/%6/H.264/media.smp")
                                    .arg(scheme,
                                         encodeUserInfo(customCctvUser),
                                         encodeUserInfo(customCctvPassword),
                                         ip,
                                         port,
                                         QString::number(i));
            config.urlLow = url;
            config.urlHigh = url; 
        } else {
            // Unchecked: rtsps?://IP:PORT/ch<ID+1>
            config.urlLow = QString("%1://%2:%3/ch%4").arg(scheme, ip, port).arg(i+1);
            config.urlHigh = QString("%1://%2:%3/ch%4_fhd").arg(scheme, ip, port).arg(i+1);
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
