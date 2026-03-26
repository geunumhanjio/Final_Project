#ifndef ROSBRIDGECLIENT_H
#define ROSBRIDGECLIENT_H

#include <QObject>
#include <QWebSocket>
#include <QPointF>
#include <QJsonObject>
#include <QJsonDocument>
#include <QVector>

class RosBridgeClient : public QObject
{
    Q_OBJECT
public:
    explicit RosBridgeClient(QObject *parent = nullptr);
    ~RosBridgeClient();

    void connectToHost(const QString &url);
    void disconnect();
    void sendCmdVel(double linear, double angular);
    void sendCameraTilt(double angle);
    void sendModeControl(const QString &mode);
    void sendNavGoto(double x, double y, double yaw);
    void sendNavPatrol(const QString &route);
    void sendNavQueue(const QVector<QPointF> &waypoints);
    void sendNavSetSpeed(double speed);
    void sendNavCancel();
    void sendGoalPose(double x, double y, double theta, const QString &frame_id = "map");

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &msg);
    void mapReceived(const QJsonObject &data);
    void odomReceived(const QJsonObject &data);
    void pathReceived(const QJsonObject &data);
    void navStatusReceived(const QJsonObject &data);
    void navFeedbackReceived(const QJsonObject &data);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);

private:
    QWebSocket m_webSocket;
};

#endif // ROSBRIDGECLIENT_H
