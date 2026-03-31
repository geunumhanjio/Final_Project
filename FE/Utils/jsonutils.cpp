#include "jsonutils.h"
#include <cmath>

QString JsonUtils::ownedTrimmedString(const QJsonValue &value)
{
    return value.toString().trimmed();
}

QPointF JsonUtils::extractPoint(const QJsonObject &data, bool *ok)
{
    if (ok) {
        *ok = false;
    }

    const QJsonObject position = data.value(QStringLiteral("position")).toObject();
    if (!position.isEmpty()) {
        if (ok) {
            *ok = position.contains(QStringLiteral("x")) || position.contains(QStringLiteral("y"));
        }
        return QPointF(position.value(QStringLiteral("x")).toDouble(),
                       position.value(QStringLiteral("y")).toDouble());
    }

    const QJsonObject translation = data.value(QStringLiteral("translation")).toObject();
    if (!translation.isEmpty()) {
        if (ok) {
            *ok = translation.contains(QStringLiteral("x")) || translation.contains(QStringLiteral("y"));
        }
        return QPointF(translation.value(QStringLiteral("x")).toDouble(),
                       translation.value(QStringLiteral("y")).toDouble());
    }

    const QJsonObject pose = data.value(QStringLiteral("pose")).toObject();
    if (!pose.isEmpty()) {
        return extractPoint(pose, ok);
    }

    const QJsonObject transform = data.value(QStringLiteral("transform")).toObject();
    if (!transform.isEmpty()) {
        return extractPoint(transform, ok);
    }

    const QJsonObject wrapperMsg = data.value(QStringLiteral("msg")).toObject();
    if (!wrapperMsg.isEmpty()) {
        return extractPoint(wrapperMsg, ok);
    }

    const QJsonObject wrapperData = data.value(QStringLiteral("data")).toObject();
    if (!wrapperData.isEmpty()) {
        return extractPoint(wrapperData, ok);
    }

    const QJsonObject wrapperPayload = data.value(QStringLiteral("payload")).toObject();
    if (!wrapperPayload.isEmpty()) {
        return extractPoint(wrapperPayload, ok);
    }

    if (data.contains(QStringLiteral("x")) || data.contains(QStringLiteral("y"))) {
        if (ok) {
            *ok = true;
        }
        return QPointF(data.value(QStringLiteral("x")).toDouble(),
                       data.value(QStringLiteral("y")).toDouble());
    }

    return QPointF();
}

double JsonUtils::extractLinearSpeed(const QJsonObject &data)
{
    const QJsonObject velocity = data.value(QStringLiteral("velocity")).toObject();
    if (!velocity.isEmpty()) {
        const double speed = extractSpeedFromVelocity(velocity, QStringLiteral("linear_x"));
        if (speed >= 0) return speed;
        
        const double linearSpeed = extractSpeedFromVelocity(velocity, QStringLiteral("linear"));
        if (linearSpeed >= 0) return linearSpeed;
        
        const double xSpeed = extractSpeedFromVelocity(velocity, QStringLiteral("x"));
        if (xSpeed >= 0) return xSpeed;
    }

    const QJsonObject twist = data.value(QStringLiteral("twist")).toObject();
    if (!twist.isEmpty()) {
        return extractLinearSpeed(twist);
    }

    const QJsonObject linear = data.value(QStringLiteral("linear")).toObject();
    if (!linear.isEmpty() && linear.contains(QStringLiteral("x"))) {
        return std::abs(linear.value(QStringLiteral("x")).toDouble());
    }

    const QJsonObject wrapperMsg = data.value(QStringLiteral("msg")).toObject();
    if (!wrapperMsg.isEmpty()) {
        return extractLinearSpeed(wrapperMsg);
    }

    const QJsonObject wrapperData = data.value(QStringLiteral("data")).toObject();
    if (!wrapperData.isEmpty()) {
        return extractLinearSpeed(wrapperData);
    }

    const QJsonObject wrapperPayload = data.value(QStringLiteral("payload")).toObject();
    if (!wrapperPayload.isEmpty()) {
        return extractLinearSpeed(wrapperPayload);
    }

    if (data.contains(QStringLiteral("speed"))) {
        return std::abs(data.value(QStringLiteral("speed")).toDouble());
    }

    return 0.0;
}

double JsonUtils::extractAngularSpeed(const QJsonObject &data)
{
    const QJsonObject velocity = data.value(QStringLiteral("velocity")).toObject();
    if (!velocity.isEmpty()) {
        const double speed = extractSpeedFromVelocity(velocity, QStringLiteral("angular_z"));
        if (speed >= 0) return speed;
        
        const double angularSpeed = extractSpeedFromVelocity(velocity, QStringLiteral("angular"));
        if (angularSpeed >= 0) return angularSpeed;
        
        const double zSpeed = extractSpeedFromVelocity(velocity, QStringLiteral("z"));
        if (zSpeed >= 0) return zSpeed;
    }

    const QJsonObject twist = data.value(QStringLiteral("twist")).toObject();
    if (!twist.isEmpty()) {
        return extractAngularSpeed(twist);
    }

    const QJsonObject angular = data.value(QStringLiteral("angular")).toObject();
    if (!angular.isEmpty() && angular.contains(QStringLiteral("z"))) {
        return std::abs(angular.value(QStringLiteral("z")).toDouble());
    }

    const QJsonObject wrapperMsg = data.value(QStringLiteral("msg")).toObject();
    if (!wrapperMsg.isEmpty()) {
        return extractAngularSpeed(wrapperMsg);
    }

    const QJsonObject wrapperData = data.value(QStringLiteral("data")).toObject();
    if (!wrapperData.isEmpty()) {
        return extractAngularSpeed(wrapperData);
    }

    const QJsonObject wrapperPayload = data.value(QStringLiteral("payload")).toObject();
    if (!wrapperPayload.isEmpty()) {
        return extractAngularSpeed(wrapperPayload);
    }

    return 0.0;
}

double JsonUtils::extractSpeedFromVelocity(const QJsonObject &velocity, const QString &key)
{
    if (velocity.contains(key)) {
        return std::abs(velocity.value(key).toDouble());
    }
    return -1.0; // Indicates key not found
}

bool JsonUtils::hasSuccessStatus(const QJsonObject &data)
{
    const QStringList stringKeys = {
        QStringLiteral("status"),
        QStringLiteral("state"),
        QStringLiteral("result"),
        QStringLiteral("outcome"),
        QStringLiteral("goal_status"),
        QStringLiteral("event"),
        QStringLiteral("message")
    };

    for (const QString &key : stringKeys) {
        const QString value = ownedTrimmedString(data.value(key)).toLower();
        if (value.isEmpty()) {
            continue;
        }
        if (value.contains(QStringLiteral("succeed"))
            || value.contains(QStringLiteral("success"))
            || value.contains(QStringLiteral("arriv"))
            || value.contains(QStringLiteral("reach"))
            || value.contains(QStringLiteral("complete"))
            || value.contains(QStringLiteral("done"))) {
            return true;
        }
    }

    const QStringList numericKeys = {
        QStringLiteral("status"),
        QStringLiteral("status_code"),
        QStringLiteral("code"),
        QStringLiteral("result_code")
    };

    for (const QString &key : numericKeys) {
        const QJsonValue value = data.value(key);
        if (!value.isDouble()) {
            continue;
        }
        const int numeric = value.toInt();
        if (numeric == 3 || numeric == 4) {
            return true;
        }
    }

    for (const QString &wrapperKey : {QStringLiteral("msg"), QStringLiteral("data"), QStringLiteral("payload")}) {
        const QJsonObject wrapper = data.value(wrapperKey).toObject();
        if (!wrapper.isEmpty() && hasSuccessStatus(wrapper)) {
            return true;
        }
    }

    return false;
}
