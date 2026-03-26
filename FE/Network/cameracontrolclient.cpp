#include "cameracontrolclient.h"
#include <QDebug>
#include <QDateTime>
#include <QFileInfo>
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
    if (cameraIp.trimmed().isEmpty()) {
        return;
    }

    if (m_webSocket.state() == QAbstractSocket::ConnectedState
        && m_webSocket.requestUrl().host() == cameraIp) {
        m_currentIp = cameraIp;
        return;
    }

    openSocketForHost(cameraIp);
}

void CameraControlClient::sendCalibrationClick(const QString &cameraIp, double x1, double y1, double x2, double y2)
{
    QJsonObject payload;
    payload["x1"] = x1;
    payload["y1"] = y1;
    payload["x2"] = x2;
    payload["y2"] = y2;

    QJsonObject msg;
    msg["type"] = "CALIBRATION_CLICK";
    msg["payload"] = payload;

    const QString jsonString = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    safeSend(jsonString, cameraIp);
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
    const QString trimmedIp = ip.trimmed();
    if (trimmedIp.isEmpty()) {
        qWarning() << "[CameraControl] Ignored command because camera IP is empty.";
        return;
    }

    m_pendingCommands.enqueue({trimmedIp, jsonString});
    processPendingCommands();
}

void CameraControlClient::onConnected()
{
    qDebug() << "[CameraControl] Connected!";
    m_isConnecting = false;
    emit connected();
    processPendingCommands();
}

void CameraControlClient::onDisconnected()
{
    qDebug() << "[CameraControl] Disconnected!";
    m_isConnecting = false;
    
    emit disconnected(); 
    
    if (!m_pendingCommands.isEmpty()) {
        QTimer::singleShot(0, this, [this]() {
            processPendingCommands();
        });
        return;
    }

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
    else if (msgType == "SLAM_MAPPING_ERROR") {
        const QJsonObject payload = obj["payload"].toObject();
        const QString reason = payload["reason"].toString().trimmed();
        const double normalizedX = payload["normalized_x"].toDouble();
        const double normalizedY = payload["normalized_y"].toDouble();

        qWarning().noquote() << QStringLiteral("[CameraControl] SLAM_MAPPING_ERROR reason=%1 normalized_x=%2 normalized_y=%3")
                                    .arg(reason.isEmpty() ? QStringLiteral("unknown") : reason)
                                    .arg(normalizedX, 0, 'f', 4)
                                    .arg(normalizedY, 0, 'f', 4);
        emit slamMappingErrorReceived(reason, normalizedX, normalizedY);
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
            emit downloadProgress(0, m_totalFileSize);
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
            emit downloadProgress(m_totalFileSize, m_totalFileSize);
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
        emit downloadProgress(m_receivedSize, m_totalFileSize);
    }
    
    // Optional: Flush every X bytes or just rely on OS
}

void CameraControlClient::processPendingCommands()
{
    if (m_pendingCommands.isEmpty()) {
        return;
    }

    const QString desiredIp = m_pendingCommands.head().cameraIp;
    const QString currentHost = m_webSocket.requestUrl().host();
    const auto socketState = m_webSocket.state();

    if (socketState == QAbstractSocket::ConnectedState) {
        if (currentHost == desiredIp) {
            while (!m_pendingCommands.isEmpty()
                   && m_webSocket.state() == QAbstractSocket::ConnectedState
                   && m_pendingCommands.head().cameraIp == desiredIp) {
                const PendingCommand command = m_pendingCommands.dequeue();
                m_webSocket.sendTextMessage(command.payload);
                qDebug() << "[CameraControl] Sent queued command to" << QString("ws://%1:9000").arg(desiredIp);
            }

            if (!m_pendingCommands.isEmpty()) {
                openSocketForHost(m_pendingCommands.head().cameraIp);
            }
            return;
        }

        openSocketForHost(desiredIp);
        return;
    }

    if (socketState == QAbstractSocket::ConnectingState) {
        if (m_currentIp != desiredIp) {
            m_webSocket.abort();
        }
        return;
    }

    openSocketForHost(desiredIp);
}

void CameraControlClient::openSocketForHost(const QString &ip)
{
    const QString trimmedIp = ip.trimmed();
    if (trimmedIp.isEmpty()) {
        return;
    }

    const QString url = QString("ws://%1:9000").arg(trimmedIp);
    const QString currentHost = m_webSocket.requestUrl().host();

    if (m_webSocket.state() == QAbstractSocket::ConnectedState) {
        m_currentIp = trimmedIp;
        if (currentHost == trimmedIp) {
            return;
        }

        qDebug() << "[CameraControl] Switching host from" << currentHost << "to" << trimmedIp;
        m_webSocket.abort();
        return;
    }

    if (m_webSocket.state() == QAbstractSocket::ConnectingState) {
        if (currentHost == trimmedIp || m_currentIp == trimmedIp) {
            m_currentIp = trimmedIp;
            return;
        }

        m_currentIp = trimmedIp;
        qDebug() << "[CameraControl] Cancelling pending connection from" << currentHost << "to" << trimmedIp;
        m_webSocket.abort();
        return;
    }

    m_currentIp = trimmedIp;
    m_isConnecting = true;
    qDebug() << "[CameraControl] Connecting to" << url;
    m_webSocket.open(QUrl(url));
}
