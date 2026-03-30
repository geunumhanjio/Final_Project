#include "rosbridgeclient.h"
#include <cmath>
#include <QDebug>
#include <QJsonArray>
#include <QDateTime>
#include <QStringList>

namespace {

QString normalizedTopicType(const QString &topic)
{
    const QString normalizedTopic = topic.trimmed().toLower();
    if (normalizedTopic.isEmpty()) {
        return {};
    }

    if (normalizedTopic.contains(QStringLiteral("nav_feedback"))) {
        return QStringLiteral("nav_feedback");
    }
    if (normalizedTopic.contains(QStringLiteral("nav_status"))) {
        return QStringLiteral("nav_status");
    }
    if (normalizedTopic.contains(QStringLiteral("fall")) || normalizedTopic.contains(QStringLiteral("alert"))) {
        return QStringLiteral("fall_alert");
    }
    if (normalizedTopic.contains(QStringLiteral("path")) || normalizedTopic.contains(QStringLiteral("plan"))) {
        return QStringLiteral("path");
    }
    if (normalizedTopic.contains(QStringLiteral("odom"))
        || normalizedTopic.contains(QStringLiteral("robot_pose"))
        || normalizedTopic.contains(QStringLiteral("amcl_pose"))
        || normalizedTopic.endsWith(QStringLiteral("/pose"))) {
        return QStringLiteral("odom");
    }
    if (normalizedTopic.contains(QStringLiteral("map"))
        || normalizedTopic.contains(QStringLiteral("occupancy_grid"))
        || normalizedTopic.contains(QStringLiteral("grid"))) {
        return QStringLiteral("map");
    }

    return {};
}

QString canonicalMessageType(QString value)
{
    value = value.trimmed().toLower();
    if (value.isEmpty()) {
        return {};
    }

    if (value.contains(QStringLiteral("nav_feedback"))) {
        return QStringLiteral("nav_feedback");
    }
    if (value.contains(QStringLiteral("nav_status"))) {
        return QStringLiteral("nav_status");
    }
    if (value.contains(QStringLiteral("fall")) || value.contains(QStringLiteral("alert"))) {
        return QStringLiteral("fall_alert");
    }
    if (value.contains(QStringLiteral("occupancygrid"))
        || value.contains(QStringLiteral("occupancy_grid"))
        || value == QStringLiteral("map")
        || value.endsWith(QStringLiteral("/map"))
        || value.contains(QStringLiteral("grid"))) {
        return QStringLiteral("map");
    }
    if (value.contains(QStringLiteral("odometry"))
        || value.contains(QStringLiteral("robot_pose"))
        || value.contains(QStringLiteral("amcl_pose"))
        || value.contains(QStringLiteral("posewithcovariance"))
        || value == QStringLiteral("odom")
        || value.endsWith(QStringLiteral("/odom"))
        || value.endsWith(QStringLiteral("/pose"))) {
        return QStringLiteral("odom");
    }
    if (value.contains(QStringLiteral("path")) || value.contains(QStringLiteral("plan"))) {
        return QStringLiteral("path");
    }

    return value;
}

QString normalizedType(const QJsonObject &object, int depth = 0)
{
    if (depth >= 4 || object.isEmpty()) {
        return {};
    }

    const QString directType = canonicalMessageType(object.value("type").toString());
    if (!directType.isEmpty()) {
        return directType;
    }

    for (const QString &key : {QStringLiteral("msg_type"),
                               QStringLiteral("message_type"),
                               QStringLiteral("datatype")}) {
        const QString alternateType = canonicalMessageType(object.value(key).toString());
        if (!alternateType.isEmpty()) {
            return alternateType;
        }
    }

    const QString op = object.value("op").toString().trimmed().toLower();
    if (op == QStringLiteral("publish") || op == QStringLiteral("message")) {
        const QString topicType = normalizedTopicType(object.value("topic").toString());
        if (!topicType.isEmpty()) {
            return topicType;
        }
    }

    for (const QString &key : {QStringLiteral("msg"), QStringLiteral("data"), QStringLiteral("payload"), QStringLiteral("values")}) {
        const QJsonObject child = object.value(key).toObject();
        if (child.isEmpty()) {
            continue;
        }

        const QString childType = normalizedType(child, depth + 1);
        if (!childType.isEmpty()) {
            return childType;
        }
    }

    return {};
}

QJsonObject unwrapMessageObject(const QJsonObject &object, int depth = 0)
{
    if (depth >= 4 || object.isEmpty()) {
        return object;
    }

    const QJsonObject values = object.value(QStringLiteral("values")).toObject();
    if (!values.isEmpty()) {
        for (const QString &key : {QStringLiteral("map"), QStringLiteral("pose"), QStringLiteral("odom")}) {
            const QJsonObject child = values.value(key).toObject();
            if (!child.isEmpty()) {
                return unwrapMessageObject(child, depth + 1);
            }
        }
        return unwrapMessageObject(values, depth + 1);
    }

    for (const QString &key : {QStringLiteral("data"), QStringLiteral("payload"), QStringLiteral("msg")}) {
        const QJsonObject child = object.value(key).toObject();
        if (!child.isEmpty()) {
            return unwrapMessageObject(child, depth + 1);
        }
    }

    return object;
}

QString compactPreview(const QJsonObject &object)
{
    QString text = QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    constexpr int maxLength = 240;
    if (text.size() > maxLength) {
        text = text.left(maxLength) + "...";
    }
    return text;
}

QString compactPreview(const QString &text)
{
    QString preview = text;
    constexpr int maxLength = 240;
    if (preview.size() > maxLength) {
        preview = preview.left(maxLength) + "...";
    }
    return preview;
}

bool looksLikeMapPayload(const QJsonObject &object)
{
    const QJsonObject info = object.value(QStringLiteral("info")).toObject();
    if (info.isEmpty()) {
        return false;
    }

    return info.value(QStringLiteral("width")).toInt() > 0
        && info.value(QStringLiteral("height")).toInt() > 0
        && info.value(QStringLiteral("resolution")).toDouble() > 0.0
        && object.value(QStringLiteral("data")).isArray();
}

bool looksLikePathPayload(const QJsonObject &object)
{
    return object.value(QStringLiteral("poses")).isArray()
        || object.value(QStringLiteral("path")).isArray()
        || object.value(QStringLiteral("points")).isArray();
}

bool looksLikeFallAlertPayload(const QJsonObject &object)
{
    if (object.isEmpty()) {
        return false;
    }

    if (object.contains(QStringLiteral("detected"))) {
        return object.contains(QStringLiteral("angle_deg"))
            || object.contains(QStringLiteral("timestamp"));
    }

    for (const QString &key : {QStringLiteral("data"), QStringLiteral("payload"), QStringLiteral("msg"), QStringLiteral("values")}) {
        const QJsonObject child = object.value(key).toObject();
        if (!child.isEmpty() && looksLikeFallAlertPayload(child)) {
            return true;
        }
    }

    return false;
}

QJsonObject extractFallAlertObject(const QJsonValue &value, int depth = 0)
{
    if (depth >= 6 || value.isUndefined() || value.isNull()) {
        return {};
    }

    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        const QString directType = object.value(QStringLiteral("type")).toString().trimmed().toLower();
        if (directType == QStringLiteral("fall_alert")) {
            const QJsonObject nestedData = object.value(QStringLiteral("data")).toObject();
            return nestedData.isEmpty() ? object : nestedData;
        }

        if (looksLikeFallAlertPayload(object)) {
            const QJsonObject nestedData = object.value(QStringLiteral("data")).toObject();
            if (!nestedData.isEmpty() && looksLikeFallAlertPayload(nestedData)) {
                return nestedData;
            }
            return object;
        }

        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            const QJsonObject nested = extractFallAlertObject(it.value(), depth + 1);
            if (!nested.isEmpty()) {
                return nested;
            }
        }
        return {};
    }

    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue &entry : array) {
            const QJsonObject nested = extractFallAlertObject(entry, depth + 1);
            if (!nested.isEmpty()) {
                return nested;
            }
        }
        return {};
    }

    if (value.isString()) {
        QJsonParseError parseError;
        const QByteArray raw = value.toString().trimmed().toUtf8();
        const QJsonDocument nestedDoc = QJsonDocument::fromJson(raw, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            return {};
        }
        if (nestedDoc.isObject()) {
            return extractFallAlertObject(nestedDoc.object(), depth + 1);
        }
        if (nestedDoc.isArray()) {
            return extractFallAlertObject(nestedDoc.array(), depth + 1);
        }
    }

    return {};
}

bool looksLikeOdomPayload(const QJsonObject &object)
{
    if (object.value(QStringLiteral("position")).isObject()
        || object.value(QStringLiteral("translation")).isObject()
        || object.value(QStringLiteral("orientation")).isObject()
        || object.contains(QStringLiteral("theta"))
        || object.contains(QStringLiteral("yaw"))
        || object.contains(QStringLiteral("heading"))) {
        return true;
    }

    const QJsonObject pose = object.value(QStringLiteral("pose")).toObject();
    if (!pose.isEmpty()) {
        return looksLikeOdomPayload(pose);
    }

    const QJsonObject transform = object.value(QStringLiteral("transform")).toObject();
    if (!transform.isEmpty()) {
        return looksLikeOdomPayload(transform);
    }

    return false;
}

QString inferredType(const QJsonObject &object, const QJsonObject &body)
{
    const QString type = normalizedType(object);
    if (!type.isEmpty()) {
        return type;
    }

    if (looksLikeFallAlertPayload(body)) {
        return QStringLiteral("fall_alert");
    }
    if (looksLikeMapPayload(body)) {
        return QStringLiteral("map");
    }
    if (looksLikePathPayload(body)) {
        return QStringLiteral("path");
    }
    if (looksLikeOdomPayload(body)) {
        return QStringLiteral("odom");
    }

    return {};
}

} // namespace

RosBridgeClient::RosBridgeClient(QObject *parent) : QObject(parent)
{
    m_reconnectTimer.setInterval(2000);
    m_reconnectTimer.setSingleShot(true);

    connect(&m_webSocket, &QWebSocket::connected, this, &RosBridgeClient::onConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &RosBridgeClient::onDisconnected);
    connect(&m_webSocket, &QWebSocket::textMessageReceived, this, &RosBridgeClient::onTextMessageReceived);
    connect(&m_webSocket, &QWebSocket::binaryMessageReceived, this, [this](const QByteArray &message) {
        onTextMessageReceived(QString::fromUtf8(message));
    });
    connect(&m_webSocket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        qWarning() << "[RosBridge] Socket error:" << m_webSocket.errorString();
        emit errorOccurred(m_webSocket.errorString());
        scheduleReconnect();
    });
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (m_manualDisconnect || m_currentUrl.trimmed().isEmpty()) {
            return;
        }

        if (m_webSocket.state() == QAbstractSocket::ConnectedState
            || m_webSocket.state() == QAbstractSocket::ConnectingState) {
            return;
        }

        qDebug() << "[RosBridge] Reconnecting to" << m_currentUrl;
        m_webSocket.open(QUrl(m_currentUrl));
    });
}

RosBridgeClient::~RosBridgeClient() {
    m_manualDisconnect = true;
    m_reconnectTimer.stop();
    m_webSocket.close();
}

void RosBridgeClient::connectToHost(const QString &url) {
    const QString trimmedUrl = url.trimmed();
    if (trimmedUrl.isEmpty()) {
        qWarning() << "[RosBridge] Refusing to connect to an empty URL.";
        return;
    }

    m_currentUrl = trimmedUrl;
    m_manualDisconnect = false;
    m_reconnectTimer.stop();

    if (m_webSocket.state() != QAbstractSocket::UnconnectedState) {
        m_webSocket.abort();
    }

    qDebug() << "[RosBridge] Connecting to" << m_currentUrl;
    m_webSocket.open(QUrl(m_currentUrl));
}

void RosBridgeClient::disconnect() {
    m_manualDisconnect = true;
    m_reconnectTimer.stop();
    m_webSocket.close();
}

void RosBridgeClient::sendCmdVel(double linear, double angular) {
    QJsonObject data;
    data["linear_x"] = linear;
    data["linear_y"] = 0.0;
    data["angular_z"] = angular;

    QJsonObject msg;
    msg["type"] = "cmd_vel";
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    msg["data"] = data;

    sendJsonMessage(msg);
}

void RosBridgeClient::sendCameraTilt(double angle)
{
    QJsonObject data;
    data["angle"] = angle;

    QJsonObject msg;
    msg["type"] = "camera_tilt";
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    msg["data"] = data;

    QString payload = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    qDebug().noquote() << "[RosBridge] Sending camera_tilt:" << payload;
    sendJsonMessage(msg);
}

void RosBridgeClient::sendModeControl(const QString &mode) {
    QJsonObject data;
    data["mode"] = mode;

    QJsonObject msg;
    msg["type"] = "mode_control";
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    msg["data"] = data;

    sendJsonMessage(msg);
}

void RosBridgeClient::sendTrackingEnable(bool enabled)
{
    QJsonObject data;
    data["enable"] = enabled;

    QJsonObject msg;
    msg["type"] = "tracking_enable";
    msg["data"] = data;

    sendJsonMessage(msg);
}

void RosBridgeClient::sendNavGoto(double x, double y, double yaw) {
    QJsonObject data;
    data["cmd"] = "goto";
    data["x"] = x;
    data["y"] = y;
    data["yaw"] = yaw;

    QJsonObject msg;
    msg["type"] = "nav_command";
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    msg["data"] = data;

    sendJsonMessage(msg);
}

void RosBridgeClient::sendNavPatrol(const QString &route) {
    QJsonObject data;
    data["cmd"] = "patrol";
    data["route"] = route;

    QJsonObject msg;
    msg["type"] = "nav_command";
    msg["data"] = data;

    sendJsonMessage(msg);
}

void RosBridgeClient::sendNavQueue(const QVector<QPointF> &waypoints)
{
    if (waypoints.isEmpty()) {
        return;
    }

    QJsonArray waypointArray;
    for (int i = 0; i < waypoints.size(); ++i) {
        double yaw = 0.0;
        if (waypoints.size() > 1) {
            QPointF direction;
            if (i + 1 < waypoints.size()) {
                direction = waypoints.at(i + 1) - waypoints.at(i);
            } else {
                direction = waypoints.at(i) - waypoints.at(i - 1);
            }

            if (!qFuzzyIsNull(direction.x()) || !qFuzzyIsNull(direction.y())) {
                yaw = std::atan2(direction.y(), direction.x());
            }
        }

        QJsonObject waypointObject;
        waypointObject["x"] = waypoints.at(i).x();
        waypointObject["y"] = waypoints.at(i).y();
        waypointObject["yaw"] = yaw;
        waypointArray.append(waypointObject);
    }

    QJsonObject data;
    data["cmd"] = "queue";
    data["waypoints"] = waypointArray;

    QJsonObject msg;
    msg["type"] = "nav_command";
    msg["data"] = data;

    sendJsonMessage(msg);
}

void RosBridgeClient::sendNavSetSpeed(double speed)
{
    QJsonObject data;
    data["cmd"] = "set_speed";
    data["speed"] = speed;

    QJsonObject msg;
    msg["type"] = "nav_command";
    msg["data"] = data;

    sendJsonMessage(msg);
}

void RosBridgeClient::sendNavCancel() {
    QJsonObject data;
    data["cmd"] = "cancel";

    QJsonObject msg;
    msg["type"] = "nav_command";
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    msg["data"] = data;

    sendJsonMessage(msg);
}
void RosBridgeClient::sendGoalPose(double x, double y, double theta, const QString &frame_id) {
    Q_UNUSED(frame_id);
    sendNavGoto(x, y, theta);
}

void RosBridgeClient::onConnected() {
    qDebug() << "[RosBridge] Connected!";
    m_reconnectTimer.stop();
    subscribeToDefaultTopics();
    emit connected();
}

void RosBridgeClient::onDisconnected() {
    qDebug() << "[RosBridge] Disconnected!";
    emit disconnected();
    scheduleReconnect();
}

void RosBridgeClient::onTextMessageReceived(const QString &message) {
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        const QString lowered = message.toLower();
        if (lowered.contains(QStringLiteral("fall")) || lowered.contains(QStringLiteral("detected")) || lowered.contains(QStringLiteral("angle_deg"))) {
            qDebug() << "[RosBridge] Fall-like raw message (non-object):" << compactPreview(message);
        }
        return;
    }

    const QJsonObject fallAlertObject = doc.isObject()
        ? extractFallAlertObject(doc.object())
        : (doc.isArray() ? extractFallAlertObject(doc.array()) : QJsonObject());
    const bool fallAlertHandled = !fallAlertObject.isEmpty();
    if (fallAlertHandled) {
        qDebug() << "[RosBridge] Fall alert message:" << compactPreview(fallAlertObject);
        emit fallAlertReceived(fallAlertObject);
    }

    if (!doc.isObject()) return;

    const QJsonObject obj = doc.object();
    const QJsonObject body = unwrapMessageObject(obj);
    const QString type = inferredType(obj, body);

    static int odomPreviewCount = 0;
    if ((type == "odom" || type == "odometry" || type == "robot_pose" || type == "pose") && odomPreviewCount < 3) {
        qDebug() << "[RosBridge] Odom message preview:" << compactPreview(body);
        ++odomPreviewCount;
    }

    static int unknownPreviewCount = 0;
    if (type.isEmpty() && unknownPreviewCount < 5) {
        qDebug() << "[RosBridge] Unhandled message preview:" << compactPreview(obj);
        ++unknownPreviewCount;
    }

    if (type == "map") {
        emit mapReceived(body);
    } else if (type == "odom" || type == "odometry" || type == "robot_pose" || type == "pose") {
        emit odomReceived(body);
    } else if (type == "path" || type == "plan") {
        emit pathReceived(body);
    } else if (type == "nav_status") {
        emit navStatusReceived(body);
    } else if (type == "nav_feedback") {
        emit navFeedbackReceived(body);
    } else if (type == "fall_alert" && !fallAlertHandled && looksLikeFallAlertPayload(body)) {
        emit fallAlertReceived(body);
    }
}

void RosBridgeClient::sendJsonMessage(const QJsonObject &message)
{
    if (m_webSocket.state() != QAbstractSocket::ConnectedState) {
        qWarning() << "[RosBridge] Dropping outbound message while disconnected:"
                   << compactPreview(message);
        return;
    }

    m_webSocket.sendTextMessage(QJsonDocument(message).toJson(QJsonDocument::Compact));
}

void RosBridgeClient::scheduleReconnect()
{
    if (m_manualDisconnect || m_currentUrl.trimmed().isEmpty()) {
        return;
    }

    if (m_webSocket.state() == QAbstractSocket::ConnectedState
        || m_webSocket.state() == QAbstractSocket::ConnectingState
        || m_reconnectTimer.isActive()) {
        return;
    }

    qDebug() << "[RosBridge] Scheduling reconnect in" << m_reconnectTimer.interval() << "ms";
    m_reconnectTimer.start();
}

void RosBridgeClient::subscribeToDefaultTopics()
{
    static const QStringList topics = {
        QStringLiteral("/map"),
        QStringLiteral("map"),
        QStringLiteral("/odom"),
        QStringLiteral("odom"),
        QStringLiteral("/robot_pose"),
        QStringLiteral("robot_pose"),
        QStringLiteral("/amcl_pose"),
        QStringLiteral("amcl_pose"),
        QStringLiteral("/path"),
        QStringLiteral("path"),
        QStringLiteral("/plan"),
        QStringLiteral("plan"),
        QStringLiteral("/global_plan"),
        QStringLiteral("global_plan"),
        QStringLiteral("/nav_status"),
        QStringLiteral("nav_status"),
        QStringLiteral("/nav_feedback"),
        QStringLiteral("nav_feedback"),
        QStringLiteral("/fall_alert"),
        QStringLiteral("fall_alert")
    };

    for (const QString &topic : topics) {
        QJsonObject subscribeMessage;
        subscribeMessage[QStringLiteral("op")] = QStringLiteral("subscribe");
        subscribeMessage[QStringLiteral("topic")] = topic;
        sendJsonMessage(subscribeMessage);
    }
}
