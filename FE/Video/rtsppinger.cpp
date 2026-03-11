#include "rtsppinger.h"

#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QSslSocket>
#include <QTcpSocket>
#include <QThread>
#include <QUrl>

#include <atomic>
#include <memory>

struct RtspSharedEndpointState {
    QString endpointKey;
    QString requestUrl;
    QString host;
    int port = 8554;
    bool isRtsps = false;
    std::atomic<bool> stopRequested { false };
    std::atomic<double> rttMs { 0.0 };
    std::atomic<int> refCount { 0 };
};

namespace {

class EndpointWorker : public QThread
{
public:
    explicit EndpointWorker(std::shared_ptr<RtspSharedEndpointState> state)
        : m_state(std::move(state))
    {
    }

protected:
    void run() override
    {
        const auto state = m_state;

        QAbstractSocket *socket = nullptr;
        if (state->isRtsps) {
            auto *sslSocket = new QSslSocket();
            sslSocket->setPeerVerifyMode(QSslSocket::VerifyNone);
            sslSocket->ignoreSslErrors();
            socket = sslSocket;
        } else {
            socket = new QTcpSocket();
        }

        socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        int cseq = 1;

        while (!state->stopRequested.load()) {
            if (socket->state() != QAbstractSocket::ConnectedState) {
                if (state->isRtsps) {
                    auto *sslSocket = static_cast<QSslSocket *>(socket);
                    sslSocket->connectToHostEncrypted(state->host, state->port);
                    if (!sslSocket->waitForConnected(3000) || !sslSocket->waitForEncrypted(3000)) {
                        state->rttMs.store(0.0);
                        socket->abort();
                        sleepWithStopCheck(state, 1000);
                        continue;
                    }
                } else {
                    socket->connectToHost(state->host, state->port);
                    if (!socket->waitForConnected(3000)) {
                        state->rttMs.store(0.0);
                        sleepWithStopCheck(state, 1000);
                        continue;
                    }
                }
            }

            const QString request = QString(
                "GET_PARAMETER %1 RTSP/1.0\r\n"
                "CSeq: %2\r\n"
                "User-Agent: VEDA-CCTV-Pinger-Shared\r\n\r\n")
                .arg(state->requestUrl)
                .arg(cseq++);

            QElapsedTimer timer;
            timer.start();

            socket->write(request.toUtf8());
            socket->flush();

            if (socket->waitForReadyRead(2000)) {
                state->rttMs.store(timer.nsecsElapsed() / 1000000.0);
                socket->readAll();
            } else {
                state->rttMs.store(0.0);
                socket->abort();
            }

            sleepWithStopCheck(state, 1000);
        }

        socket->abort();
        delete socket;
    }

private:
    static void sleepWithStopCheck(const std::shared_ptr<RtspSharedEndpointState> &state, int totalMs)
    {
        constexpr int kChunkMs = 100;
        int remaining = totalMs;
        while (remaining > 0 && !state->stopRequested.load()) {
            const int sleepMs = qMin(kChunkMs, remaining);
            QThread::msleep(static_cast<unsigned long>(sleepMs));
            remaining -= sleepMs;
        }
    }

    std::shared_ptr<RtspSharedEndpointState> m_state;
};

QMutex g_endpointMutex;
QHash<QString, std::shared_ptr<RtspSharedEndpointState>> g_endpointStates;
QHash<QString, EndpointWorker *> g_endpointWorkers;

QString endpointKeyForUrl(const QUrl &url)
{
    const int port = url.port(url.scheme() == "rtsps" ? 8322 : 8554);
    return QString("%1://%2:%3")
        .arg(url.scheme().toLower(), url.host().toLower())
        .arg(port);
}

} // namespace

RtspPinger::RtspPinger(QObject *parent)
    : QObject(parent)
{
}

RtspPinger::~RtspPinger()
{
    stop();
}

void RtspPinger::startPinger(const QString &urlStr)
{
    stop();

    const QUrl url(urlStr);
    if (!url.isValid() || url.host().isEmpty()) {
        return;
    }

    const QString endpointKey = endpointKeyForUrl(url);
    std::shared_ptr<RtspSharedEndpointState> state;
    EndpointWorker *worker = nullptr;

    {
        QMutexLocker locker(&g_endpointMutex);

        state = g_endpointStates.value(endpointKey);
        if (!state) {
            state = std::make_shared<RtspSharedEndpointState>();
            state->endpointKey = endpointKey;
            state->requestUrl = urlStr;
            state->host = url.host();
            state->port = url.port(url.scheme() == "rtsps" ? 8322 : 8554);
            state->isRtsps = (url.scheme().compare("rtsps", Qt::CaseInsensitive) == 0);
            g_endpointStates.insert(endpointKey, state);
        }

        worker = g_endpointWorkers.value(endpointKey, nullptr);
        if (!worker) {
            worker = new EndpointWorker(state);
            QObject::connect(worker, &QThread::finished, worker, &QObject::deleteLater);
            g_endpointWorkers.insert(endpointKey, worker);
        }

        state->stopRequested.store(false);
        state->refCount.fetch_add(1);
    }

    if (worker && !worker->isRunning()) {
        worker->start(QThread::LowestPriority);
    }

    m_endpointKey = endpointKey;
    m_state = std::move(state);
}

void RtspPinger::stop()
{
    if (!m_state) {
        return;
    }

    std::shared_ptr<RtspSharedEndpointState> state = m_state;
    const QString endpointKey = m_endpointKey;

    m_state.reset();
    m_endpointKey.clear();

    QMutexLocker locker(&g_endpointMutex);
    const int remainingRefs = state->refCount.fetch_sub(1) - 1;
    if (remainingRefs <= 0) {
        state->refCount.store(0);
        state->stopRequested.store(true);
        g_endpointStates.remove(endpointKey);
        g_endpointWorkers.remove(endpointKey);
    }
}

double RtspPinger::getRttMs() const
{
    return m_state ? m_state->rttMs.load() : 0.0;
}
