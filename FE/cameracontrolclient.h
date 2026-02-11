#ifndef CAMERACONTROLCLIENT_H
#define CAMERACONTROLCLIENT_H

#include <QObject>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDir>

class CameraControlClient : public QObject
{
    Q_OBJECT
public:
    explicit CameraControlClient(QObject *parent = nullptr);
    ~CameraControlClient();

    // Connects to ws://[cameraIp]:9000 and sends the command
    void sendRecordCommand(const QString &cameraIp, int channelId, bool start);
    void requestRecordings(const QString &cameraIp);
    void requestDownload(const QString &cameraIp, const QString &filename); // [New]

signals:
    void connected();
    void disconnected();
    void errorOccurred(QString error);
    void videoReceived(QString url);
    void recordingListReceived(QJsonArray list);
    
    // File Transfer Signals
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(QString filePath);

private slots:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onTextMessageReceived(const QString &message);
    void onBinaryMessageReceived(const QByteArray &message); // [New]

private:
    QWebSocket m_webSocket;
    QString m_pendingCommand;
    bool m_isConnecting;

    // File Transfer State
    QFile m_downloadFile;
    bool m_isDownloading = false;
    qint64 m_totalFileSize = 0;
    qint64 m_receivedSize = 0;
    
    // Helper to ensure command delivery
    void safeSend(const QString &jsonString, const QString &ip);
};

#endif // CAMERACONTROLCLIENT_H
