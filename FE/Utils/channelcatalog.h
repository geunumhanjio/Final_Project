#ifndef CHANNELCATALOG_H
#define CHANNELCATALOG_H

#include <QList>
#include <QString>

struct VideoChannelDefinition
{
    int viewIndex = -1;
    int liveRecordChannelId = -1;
    int fullScreenRecordChannelId = -1;
    QString liveCardTitle;
    QString sidebarTitle;
};

struct PlaybackCategoryDefinition
{
    int id = -1;
    QString name;
};

class ChannelCatalog
{
public:
    static const QList<VideoChannelDefinition> &videoChannels();
    static const QList<PlaybackCategoryDefinition> &playbackCategories();

    static const VideoChannelDefinition *findVideoChannel(int viewIndex);
    static int liveRecordChannelIdForIndex(int viewIndex);
    static int fullScreenRecordChannelIdForIndex(int viewIndex);
    static QString liveCardTitleForIndex(int viewIndex);
    static QString sidebarTitleForIndex(int sidebarIndex);
    static QString playbackCategoryName(int categoryId);
    static int parseRecordingChannelId(const QString &fileName);
};

#endif // CHANNELCATALOG_H
