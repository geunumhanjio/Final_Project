#ifndef RTSPPINGER_H
#define RTSPPINGER_H

#include <QObject>
#include <QString>
#include <memory>

struct RtspSharedEndpointState;

class RtspPinger : public QObject
{
    Q_OBJECT

public:
    explicit RtspPinger(QObject *parent = nullptr);
    ~RtspPinger();

    void startPinger(const QString &urlStr);
    void stop();
    double getRttMs() const;

private:
    QString m_endpointKey;
    std::shared_ptr<RtspSharedEndpointState> m_state;
};

#endif // RTSPPINGER_H
