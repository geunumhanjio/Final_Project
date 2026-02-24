#include "cameracontrolclient.h"
#include <QDebug>
#include <QDateTime>
#include <QStandardPaths> // [New]
#include <QTimer> // [Fix] Include QTimer for singleShot

CameraControlClient::CameraControlClient(QObject *parent) : QObject(parent)
{
    m_isConnecting = false;

    connect(&m_webSocket, &QWebSocket::connected, this, &CameraControlClient::onConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &CameraControlClient::onDisconnected);
    // QWebSocket::error is overloaded, so we need static_cast or lambda.
    connect(&m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &CameraControlClient::onError);
    connect(&m_webSocket, &QWebSocket::textMessageReceived, this, &CameraControlClient::onTextMessageReceived);
    connect(&m_webSocket, &QWebSocket::binaryMessageReceived, this, &CameraControlClient::onBinaryMessageReceived); // [New]
}

CameraControlClient::~CameraControlClient()
{
    m_currentIp.clear();
    m_webSocket.close();
}

void CameraControlClient::connectToServer(const QString &cameraIp)
{
    // If different IP, or not connected, establish connection
    if (m_currentIp != cameraIp || m_webSocket.state() == QAbstractSocket::UnconnectedState) {
        m_webSocket.close();
        m_currentIp = cameraIp;
        m_isConnecting = true;
        QString url = QString("ws://%1:9000").arg(cameraIp);
        qDebug() << "[CameraControl] Persistent connect to" << url;
        m_webSocket.open(QUrl(url));
    }
}

void CameraControlClient::sendRecordCommand(const QString &cameraIp, int channelId, bool start)
{
    // Construct JSON Payload
    QJsonObject payload;
    payload["action"] = start ? "start" : "stop";
    payload["channel_id"] = channelId;

    QJsonObject msg;
    msg["type"] = "RECORD_CONTROL";
    msg["payload"] = payload;
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch() / 1000.0;

    QString jsonString = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    safeSend(jsonString, cameraIp);
}

void CameraControlClient::requestRecordings(const QString &cameraIp)
{
    QJsonObject msg;
    msg["type"] = "GET_RECORDINGS";
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch() / 1000.0;

    QString jsonString = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    safeSend(jsonString, cameraIp);
}

void CameraControlClient::requestDownload(const QString &cameraIp, const QString &filename)
{
    QJsonObject msg;
    msg["type"] = "DOWNLOAD_FILE";
    msg["payload"] = QJsonObject{ {"filename", filename} };
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch() / 1000.0;

    QString jsonString = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    safeSend(jsonString, cameraIp);
}

void CameraControlClient::safeSend(const QString &jsonString, const QString &ip)
{
    QString url = QString("ws://%1:9000").arg(ip);
    m_currentIp = ip; // Keep track of latest IP
    
    // If connected to the same IP, assume good state and send
    if (m_webSocket.state() == QAbstractSocket::ConnectedState) {
        if (m_webSocket.requestUrl().host() == ip) {
            m_webSocket.sendTextMessage(jsonString);
            qDebug() << "[CameraControl] Sent command to" << url;
            return;
        } else {
            qDebug() << "[CameraControl] Switching host from" << m_webSocket.requestUrl().host() << "to" << ip;
            m_webSocket.close();
        }
    }

    // If connecting or unconnected, queue the command
    m_pendingCommand = jsonString;
    
    // Only call open if not already connecting to the same target
    // But QWebSocket doesn't expose 'target' while connecting easily.
    // Simplest approach: If not Connected, call open (it handles state checks mostly, but we'll reset)
    if (m_webSocket.state() != QAbstractSocket::ConnectingState) {
        m_isConnecting = true;
        qDebug() << "[CameraControl] Connecting to" << url;
        m_webSocket.open(QUrl(url));
    } else {
        qDebug() << "[CameraControl] Already connecting, queued command.";
    }
}

void CameraControlClient::onConnected()
{
    qDebug() << "[CameraControl] Connected!";
    m_isConnecting = false;
    emit connected();

    if (!m_pendingCommand.isEmpty()) {
        m_webSocket.sendTextMessage(m_pendingCommand); // Direct send
        qDebug() << "[CameraControl] Sent pending command:" << m_pendingCommand;
        m_pendingCommand.clear();
    }
}

void CameraControlClient::onDisconnected()
{
    qDebug() << "[CameraControl] Disconnected!";
    m_isConnecting = false;
    
    emit disconnected(); 
    
    // Auto Reconnect logic
    if (!m_currentIp.isEmpty()) {
        qDebug() << "[CameraControl] Auto reconnecting to" << m_currentIp << "in 3 seconds...";
        QTimer::singleShot(3000, this, [this]() {
            if (!m_currentIp.isEmpty() && m_webSocket.state() == QAbstractSocket::UnconnectedState) {
                connectToServer(m_currentIp);
            }
        });
    }
}

void CameraControlClient::onError(QAbstractSocket::SocketError error)
{
    qDebug() << "[CameraControl] Error:" << m_webSocket.errorString();
    emit errorOccurred(m_webSocket.errorString());
    m_isConnecting = false;
}

void CameraControlClient::onTextMessageReceived(const QString &message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isNull()) return;

    QJsonObject obj = doc.object();
    QString msgType = obj["type"].toString();

    // [New] Stream Stats Parsing
    if (msgType == "STREAM_STATS") {
        QJsonObject payload = obj["payload"].toObject();
        int channelId = payload["channel_id"].toInt();
        double fps = payload["fps"].toDouble();
        double bitrateKbps = payload["bitrate_kbps"].toDouble();
        double proxyLatencyMs = payload["proxy_latency_ms"].toDouble();
        
        emit streamStatsReceived(channelId, fps, bitrateKbps, proxyLatencyMs);
        return;
    }

    if (msgType == "RECORD_FINISHED") {
        QJsonObject payload = obj["payload"].toObject();
        QString url = payload["url"].toString();
        if (!url.isEmpty()) {
            qDebug() << "[CameraControl] Video Received:" << url;
            emit videoReceived(url);
        }
    }
    else if (msgType == "RECORDING_LIST") {
        QJsonArray list = obj["payload"].toArray();
        qDebug() << "[CameraControl] Recording List Received:" << list.size() << "items";
        emit recordingListReceived(list);
    }
    // [New] File Transfer Protocol
    else if (msgType == "FILE_TRANSFER_START") {
        QJsonObject payload = obj["payload"].toObject();
        QString filename = payload["filename"].toString();
        m_totalFileSize = payload["file_size"].toDouble(); 
        m_receivedSize = 0;
        
        QString savePath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        QDir().mkpath(savePath);
        m_downloadFile.setFileName(savePath + "/" + filename);
        
        if (m_downloadFile.open(QIODevice::WriteOnly)) {
            m_isDownloading = true;
            qDebug() << "[Downloader] Start receiving:" << filename << "Expected Size:" << m_totalFileSize;
        } else {
            qCritical() << "[Downloader] Error opening file:" << m_downloadFile.errorString();
            emit errorOccurred("File open failed: " + m_downloadFile.errorString());
        }
    }
    else if (msgType == "FILE_TRANSFER_COMPLETE") {
        qDebug() << "[Downloader] Transfer Complete Signal Received. Downloaded:" << m_receivedSize;
        if (m_isDownloading) {
            m_downloadFile.close();
            m_isDownloading = false;
            QString filePath = m_downloadFile.fileName();
            qDebug() << "[Downloader] File Saved:" << filePath << "Size:" << QFileInfo(filePath).size();
            emit downloadFinished(filePath);
        } else {
            qWarning() << "[Downloader] Received COMPLETE signal but not downloading!";
        }
    }
    // ... existing error handling ...
}

void CameraControlClient::onBinaryMessageReceived(const QByteArray &message)
{
    // qDebug() << "[Downloader] Binary Chunk Received:" << message.size() << "bytes"; // Validating receipt
    
    if (!m_isDownloading) {
        qWarning() << "[Downloader] Ignored binary data (Not in downloading state). Size:" << message.size();
        return;
    }
    
    if (!m_downloadFile.isOpen()) {
         qCritical() << "[Downloader] File not open for writing!";
         return;
    }

    qint64 written = m_downloadFile.write(message);
    if (written == -1) {
        qCritical() << "[Downloader] Write Error:" << m_downloadFile.errorString();
    } else {
        m_receivedSize += written;
        // emit downloadProgress(m_receivedSize, m_totalFileSize); // Reduce spam
    }
    
    // Optional: Flush every X bytes or just rely on OS
}
