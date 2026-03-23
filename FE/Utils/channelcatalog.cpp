#include "channelcatalog.h"

namespace {

const QList<VideoChannelDefinition> kVideoChannels = {
    {0, 1, 5, QStringLiteral("Channel 01 - Camera"), QStringLiteral("Channel 01 - Camera")},
    {1, 2, 6, QStringLiteral("Channel 02 - Camera"), QStringLiteral("Channel 02 - Camera")},
    {2, 3, 7, QStringLiteral("Channel 03 - Camera"), QStringLiteral("Channel 03 - Camera")},
    {3, 4, 8, QStringLiteral("Channel 04 - Camera"), QStringLiteral("Channel 04 - Camera")},
    {4, 9, 9, QStringLiteral("RC Car - Front Cam"), QStringLiteral("RC Front Cam")}
};

const QList<PlaybackCategoryDefinition> kPlaybackCategories = {
    {0, QStringLiteral("All Recordings")},
    {1, QStringLiteral("CCTV 1 (Low)")},
    {2, QStringLiteral("CCTV 2 (Low)")},
    {3, QStringLiteral("CCTV 3 (Low)")},
    {4, QStringLiteral("CCTV 4 (Low)")},
    {5, QStringLiteral("CCTV 1 (High)")},
    {6, QStringLiteral("CCTV 2 (High)")},
    {7, QStringLiteral("CCTV 3 (High)")},
    {8, QStringLiteral("CCTV 4 (High)")},
    {9, QStringLiteral("RC Car Camera")},
    {10, QStringLiteral("Lidar Map")}
};

} // namespace

const QList<VideoChannelDefinition> &ChannelCatalog::videoChannels()
{
    return kVideoChannels;
}

const QList<PlaybackCategoryDefinition> &ChannelCatalog::playbackCategories()
{
    return kPlaybackCategories;
}

const VideoChannelDefinition *ChannelCatalog::findVideoChannel(int viewIndex)
{
    for (const VideoChannelDefinition &definition : kVideoChannels) {
        if (definition.viewIndex == viewIndex) {
            return &definition;
        }
    }

    return nullptr;
}

int ChannelCatalog::liveRecordChannelIdForIndex(int viewIndex)
{
    if (const VideoChannelDefinition *definition = findVideoChannel(viewIndex)) {
        return definition->liveRecordChannelId;
    }

    return -1;
}

int ChannelCatalog::fullScreenRecordChannelIdForIndex(int viewIndex)
{
    if (const VideoChannelDefinition *definition = findVideoChannel(viewIndex)) {
        return definition->fullScreenRecordChannelId;
    }

    return -1;
}

QString ChannelCatalog::liveCardTitleForIndex(int viewIndex)
{
    if (const VideoChannelDefinition *definition = findVideoChannel(viewIndex)) {
        return definition->liveCardTitle;
    }

    return QString();
}

QString ChannelCatalog::sidebarTitleForIndex(int sidebarIndex)
{
    if (sidebarIndex == 5) {
        return QStringLiteral("Lidar Map Stream");
    }

    if (const VideoChannelDefinition *definition = findVideoChannel(sidebarIndex)) {
        return definition->sidebarTitle;
    }

    return QString();
}

QString ChannelCatalog::playbackCategoryName(int categoryId)
{
    for (const PlaybackCategoryDefinition &definition : kPlaybackCategories) {
        if (definition.id == categoryId) {
            return definition.name;
        }
    }

    return QStringLiteral("Unknown Category");
}

int ChannelCatalog::parseRecordingChannelId(const QString &fileName)
{
    if (!fileName.startsWith(QStringLiteral("rec_ch"))) {
        return -1;
    }

    const int start = 6;
    const int end = fileName.indexOf(QLatin1Char('_'), start);
    if (end <= start) {
        return -1;
    }

    bool ok = false;
    const int channelId = fileName.mid(start, end - start).toInt(&ok);
    return ok ? channelId : -1;
}
