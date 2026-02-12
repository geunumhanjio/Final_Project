#ifndef STREAMMANAGER_H
#define STREAMMANAGER_H

#include <QObject>
#include <QString>
#include <QList>

struct ChannelConfig {
    int id;
    QString name;
    QString urlLow;
    QString urlHigh;
    bool isActive;
};

class StreamManager : public QObject
{
    Q_OBJECT
public:
    static StreamManager& instance() {
        static StreamManager _instance;
        return _instance;
    }

    void loadConfig();
    QList<ChannelConfig> getChannels() const { return m_channels; }
    QString getLowQualityUrl(int index) const;
    QString getHighQualityUrl(int index) const;

signals:
    void configLoaded();

private:
    explicit StreamManager(QObject *parent = nullptr);
    QList<ChannelConfig> m_channels;
};

#endif // STREAMMANAGER_H
