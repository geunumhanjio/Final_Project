#include "rosbridgeclient.h"
#include <QDebug>
#include <QDateTime>

RosBridgeClient::RosBridgeClient(QObject *parent) : QObject(parent)
{
    connect(&m_webSocket, &QWebSocket::connected, this, &RosBridgeClient::onConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &RosBridgeClient::onDisconnected);
    connect(&m_webSocket, &QWebSocket::textMessageReceived, this, &RosBridgeClient::onTextMessageReceived);
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
    data["angular_z"] = angular;

    QJsonObject msg;
    msg["type"] = "cmd_vel";
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    msg["data"] = data;

    m_webSocket.sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}
void RosBridgeClient::sendGoalPose(double x, double y, double theta, const QString &frame_id) {
    if (m_webSocket.state() != QAbstractSocket::ConnectedState) return;

    QJsonObject data;
    data["x"] = x;
    data["y"] = y;
    data["theta"] = theta;
    data["frame_id"] = frame_id;

    QJsonObject msg;
    msg["type"] = "goal_pose";
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    msg["data"] = data;

    m_webSocket.sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
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
    const QString type = obj.value("type").toString();
    const QJsonObject data = obj.value("data").toObject();

    if (type == "map") {
        emit mapReceived(data);
    } else if (type == "odom") {
        emit odomReceived(data);
    } else if (type == "path") {
        emit pathReceived(data);
    }
}
