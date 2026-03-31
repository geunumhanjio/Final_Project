#ifndef JSONUTILS_H
#define JSONUTILS_H

#include <QJsonObject>
#include <QPointF>
#include <QJsonValue>

class JsonUtils
{
public:
    static QString ownedTrimmedString(const QJsonValue &value);
    static QPointF extractPoint(const QJsonObject &data, bool *ok = nullptr);
    static double extractLinearSpeed(const QJsonObject &data);
    static double extractAngularSpeed(const QJsonObject &data);
    static bool hasSuccessStatus(const QJsonObject &data);
    
private:
    static double extractSpeedFromVelocity(const QJsonObject &velocity, const QString &key);
};

#endif // JSONUTILS_H
