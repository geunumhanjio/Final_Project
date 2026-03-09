#ifndef RTSPPINGER_H
#define RTSPPINGER_H

#include <QThread>
#include <QTcpSocket>
#include <QSslSocket>
#include <QElapsedTimer>
#include <QUrl>
#include <QDebug>
#include <atomic>

class RtspPinger : public QThread {
    Q_OBJECT
public:
    explicit RtspPinger(QObject *parent = nullptr) 
        : QThread(parent), m_stop(false), m_rtt(0.0) {}

    ~RtspPinger() {
        stop();
    }

    void startPinger(const QString &urlStr) {
        stop();
        m_url = urlStr;
        m_stop = false;
        start(QThread::LowestPriority); // UI보다 낮은 우선순위로 안정적 실행
    }

    void stop() {
        m_stop = true;
        wait(); // 스레드 종료 대기
    }

    double getRttMs() const { 
        return m_rtt.load(); 
    }

protected:
    void run() override {
        QUrl qurl(m_url);
        QString host = qurl.host();
        int port = qurl.port() > 0 ? qurl.port() : 8554;
        bool isRtsps = (qurl.scheme() == "rtsps");

        QTcpSocket* socket = nullptr;
        if (isRtsps) {
            QSslSocket* sslSocket = new QSslSocket();
            sslSocket->setPeerVerifyMode(QSslSocket::VerifyNone);
            sslSocket->ignoreSslErrors();
            socket = sslSocket;
        } else {
            socket = new QTcpSocket();
        }

        socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        int cseq = 1;

        while (!m_stop) {
            // 1. 연결 확인 및 수행
            if (socket->state() != QAbstractSocket::ConnectedState) {
                if (isRtsps) 
                    static_cast<QSslSocket*>(socket)->connectToHostEncrypted(host, port);
                else 
                    socket->connectToHost(host, port);

                if (!socket->waitForConnected(3000)) {
                    m_rtt.store(0.0);
                    QThread::msleep(1000);
                    continue;
                }

                if (isRtsps) {
                    if (!static_cast<QSslSocket*>(socket)->waitForEncrypted(3000)) {
                        m_rtt.store(0.0);
                        socket->abort();
                        continue;
                    }
                }
            }

            // 2. 핑 송신 (GET_PARAMETER)
            QString request = QString("GET_PARAMETER %1 RTSP/1.0\r\n"
                                      "CSeq: %2\r\n"
                                      "User-Agent: VEDA-CCTV-Pinger-Thread\r\n\r\n")
                              .arg(m_url).arg(cseq++);
            
            QElapsedTimer timer;
            timer.start();
            
            socket->write(request.toUtf8());
            socket->flush();

            // 3. 응답 대기 (블로킹 방식 - 매우 정밀함)
            if (socket->waitForReadyRead(2000)) {
                double elapsed = timer.nsecsElapsed() / 1000000.0; // 나노초 단위 측정 후 ms 변환
                m_rtt.store(elapsed);
                socket->readAll(); // 버퍼 비우기
            } else {
                m_rtt.store(0.0);
                socket->abort(); // 타임아웃 시 연결 리셋
            }

            // 4. 다음 측정까지 대기 (1초 주기)
            QThread::msleep(1000);
        }

        socket->abort();
        delete socket;
    }

private:
    QString m_url;
    std::atomic<bool> m_stop;
    std::atomic<double> m_rtt;
};

#endif // RTSPPINGER_H
