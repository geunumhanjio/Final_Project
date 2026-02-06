#include "liveview.h"
#include <QDebug>
#include <QTimer>

LiveView::LiveView(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    this->setStyleSheet("background-color: #000000;");

    gridLayout = new QGridLayout(this);
    gridLayout->setSpacing(5);
    gridLayout->setContentsMargins(5, 5, 5, 5);

    for(int i = 0; i < 4; i++) {
        cctvWidgets[i] = new VideoWidget(this);
        cctvWidgets[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        cctvWidgets[i]->setMinimumSize(320, 240);
        cctvWidgets[i]->installEventFilter(this);
        gridLayout->addWidget(cctvWidgets[i], i / 2, i % 2);
    }

    for(int i = 0; i < 2; i++) {
        QString name = (i == 0) ? "RC Car Camera" : "LiDAR SLAM Map";
        sensorWidgets[i] = new QLabel(name, this);
        sensorWidgets[i]->setAlignment(Qt::AlignCenter);
        sensorWidgets[i]->setStyleSheet("QLabel { border: 2px solid #00FFFF; color: white; background-color: #111; font-size: 20px; }");
        sensorWidgets[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sensorWidgets[i]->installEventFilter(this);
        gridLayout->addWidget(sensorWidgets[i], i, 2);
    }

    if (!gst_is_initialized()) gst_init(nullptr, nullptr);
    streamStarted = false;
}

void LiveView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!streamStarted) {
        QTimer::singleShot(500, this, [this]() { initCCTVStreams(); });
        streamStarted = true;
    }
}

void LiveView::initCCTVStreams()
{
    qDebug() << "[LiveView] Initializing RTSP connections...";

    lowQualityUrls.clear();
    highQualityUrls.clear();

    // =================================================================
    // [옵션 A] 라즈베리파이 / 내 서버 (활성화됨)
    // =================================================================

    QString ip = "192.168.0.38";
    QString port = "8554";

    for(int i = 0; i < 4; i++) {
        // 1. 저화질 (4분할 그리드용): ch1, ch2, ch3...
        QString lowUrl = QString("rtsp://%1:%2/ch%3").arg(ip, port).arg(i+1);
        lowQualityUrls << lowUrl;

        // 2. 고화질 (전체화면용): ch1_fhd, ch2_fhd...
        QString highUrl = QString("rtsp://%1:%2/ch%3_fhd").arg(ip, port).arg(i+1);
        highQualityUrls << highUrl;
    }
    qDebug() << "[Mode] Raspberry Pi Server Selected (Dual Stream: Normal/FHD)";

    /*
    // =================================================================
    // [옵션 B] 상용 CCTV (한화) - 듀얼 스트림 사용 (현재 활성화)
    // =================================================================
    //
    QString baseUrl = "rtsp://admin:5hanwha!@192.168.0.16:554";
    for(int i = 0; i < 4; i++) {
        // 저화질 (MOBILE) -> 4분할 화면용
        lowQualityUrls << QString("%1/%2/MOBILE/media.smp").arg(baseUrl).arg(i);
        // 고화질 (H.264) -> 전체 화면용
        highQualityUrls << QString("%1/%2/H.264/media.smp").arg(baseUrl).arg(i);
    }
    qDebug() << "[Mode] Commercial CCTV Selected (Dual Stream)";
    //
    */

    for(int i = 0; i < 4; i++) {
        if (i >= lowQualityUrls.size()) break;
        QString url = lowQualityUrls[i];
        int delay = i * 100;

        QTimer::singleShot(delay, this, [this, i, url]() {
            if(cctvWidgets[i]) {
                cctvWidgets[i]->playUrl(url, 200);
            }
        });
    }
}

bool LiveView::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::MouseButtonDblClick) {
        for (int i = 0; i < 4; i++) {
            if (obj == cctvWidgets[i]) {
                QString url = "";
                if (i < highQualityUrls.size()) url = highQualityUrls[i];
                emit requestFullScreen(i, url);
                return true;
            }
        }
        for (int i = 0; i < 2; i++) {
            if (obj == sensorWidgets[i]) {
                emit requestFullScreen(i + 4, "");
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void LiveView::setChannelVisible(int index, bool visible) {
    if (index < 4) cctvWidgets[index]->setVisible(visible);
    else if (index < 6) sensorWidgets[index - 4]->setVisible(visible);
}

void LiveView::stopAll() {
    for(int i = 0; i < 4; i++) if (cctvWidgets[i]) cctvWidgets[i]->stop();
}
