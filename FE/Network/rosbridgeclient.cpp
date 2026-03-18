#include "rosbridgeclient.h"
#include <QDebug>
#include <QDateTime>

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

QString normalizedType(const QJsonObject &object)
{
    const QString directType = object.value("type").toString().trimmed().toLower();
    if (!directType.isEmpty()) {
        return directType;
    }

    const QString op = object.value("op").toString().trimmed().toLower();
    if (op == QStringLiteral("publish") || op == QStringLiteral("message")) {
        return normalizedTopicType(object.value("topic").toString());
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
    connect(&m_webSocket, &QWebSocket::connected, this, &RosBridgeClient::onConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &RosBridgeClient::onDisconnected);
    connect(&m_webSocket, &QWebSocket::textMessageReceived, this, &RosBridgeClient::onTextMessageReceived);
    connect(&m_webSocket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        qWarning() << "[RosBridge] Socket error:" << m_webSocket.errorString();
        emit errorOccurred(m_webSocket.errorString());
    });
}

RosBridgeClient::~RosBridgeClient() {
    m_webSocket.close();
}

void RosBridgeClient::connectToHost(const QString &url) {
    qDebug() << "[RosBridge] Connecting to" << url;
    m_webSocket.open(QUrl(url));
}

void RosBridgeClient::disconnect() {
    m_webSocket.close();
}

void RosBridgeClient::sendCmdVel(double linear, double angular) {
    if (m_webSocket.state() != QAbstractSocket::ConnectedState) return;

    QJsonObject data;
    data["linear_x"] = linear;
    data["linear_y"] = 0.0;
    data["angular_z"] = angular;

    QJsonObject msg;
    msg["type"] = "cmd_vel";
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    msg["data"] = data;

    m_webSocket.sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void RosBridgeClient::sendModeControl(const QString &mode) {
    if (m_webSocket.state() != QAbstractSocket::ConnectedState) return;

    QJsonObject data;
    data["mode"] = mode;

    QJsonObject msg;
    msg["type"] = "mode_control";
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    msg["data"] = data;

    m_webSocket.sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void RosBridgeClient::sendNavGoto(double x, double y, double yaw) {
    if (m_webSocket.state() != QAbstractSocket::ConnectedState) return;

    QJsonObject data;
    data["cmd"] = "goto";
    data["x"] = x;
    data["y"] = y;
    data["yaw"] = yaw;

    QJsonObject msg;
    msg["type"] = "nav_command";
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    msg["data"] = data;

    m_webSocket.sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void RosBridgeClient::sendNavGotoWaypoint(const QString &name) {
    if (m_webSocket.state() != QAbstractSocket::ConnectedState) return;

    QJsonObject data;
    data["cmd"] = "goto_wp";
    data["name"] = name;

    QJsonObject msg;
    msg["type"] = "nav_command";
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    msg["data"] = data;

    m_webSocket.sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void RosBridgeClient::sendNavPatrol(const QString &route) {
    if (m_webSocket.state() != QAbstractSocket::ConnectedState) return;

    QJsonObject data;
    data["cmd"] = "patrol";
    data["route"] = route;

    QJsonObject msg;
    msg["type"] = "nav_command";
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    msg["data"] = data;

    m_webSocket.sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void RosBridgeClient::sendNavCancel() {
    if (m_webSocket.state() != QAbstractSocket::ConnectedState) return;

    QJsonObject data;
    data["cmd"] = "cancel";

    QJsonObject msg;
    msg["type"] = "nav_command";
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    msg["data"] = data;

    m_webSocket.sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void RosBridgeClient::sendGoalPose(double x, double y, double theta, const QString &frame_id) {
    Q_UNUSED(frame_id);
    sendNavGoto(x, y, theta);
}

void RosBridgeClient::emergencyStop() {
    if (m_webSocket.state() != QAbstractSocket::ConnectedState) return;

    QJsonObject data;
    data["stop"] = true;

    QJsonObject msg;
    msg["type"] = "emergency_stop";
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    msg["data"] = data;

    m_webSocket.sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void RosBridgeClient::onConnected() {
    qDebug() << "[RosBridge] Connected!";
    emit connected();
}

void RosBridgeClient::onDisconnected() {
    qDebug() << "[RosBridge] Disconnected!";
    emit disconnected();
}

void RosBridgeClient::onTextMessageReceived(const QString &message) {
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
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
    }
}
