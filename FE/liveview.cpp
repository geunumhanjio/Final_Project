/**
 * @file liveview.cpp
 * @brief 화면 배치 및 RTSP 주소 연결 구현
 */
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

    // 1. CCTV 4개 생성 및 배치
    for(int i = 0; i < 4; i++) {
        cctvWidgets[i] = new VideoWidget(this);
        cctvWidgets[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        cctvWidgets[i]->setMinimumSize(320, 240);
        cctvWidgets[i]->installEventFilter(this);

        int row = i / 2;
        int col = i % 2;
        gridLayout->addWidget(cctvWidgets[i], row, col);
    }

    // 2. 센서 화면 2개 생성
    for(int i = 0; i < 2; i++) {
        QString name = (i == 0) ? "RC Car Camera" : "LiDAR SLAM Map";
        sensorWidgets[i] = new QLabel(name, this);
        sensorWidgets[i]->setAlignment(Qt::AlignCenter);
        sensorWidgets[i]->setStyleSheet("QLabel { border: 2px solid #00FFFF; color: white; background-color: #111; font-size: 20px; }");
        sensorWidgets[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sensorWidgets[i]->installEventFilter(this);

        gridLayout->addWidget(sensorWidgets[i], i, 2);
    }

    // GStreamer 초기화
    if (!gst_is_initialized()) {
        qDebug() << "[LiveView] Initializing GStreamer...";
        gst_init(nullptr, nullptr);

        guint major, minor, micro, nano;
        gst_version(&major, &minor, &micro, &nano);
        qDebug() << "[LiveView] GStreamer version:" << major << "." << minor << "." << micro;
    }

    streamStarted = false;
}

void LiveView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    if (!streamStarted) {
        qDebug() << "[LiveView] Widget visible, starting streams in 1 second...";

        QTimer::singleShot(1000, this, [this]() {
            initCCTVStreams();
        });

        streamStarted = true;
    }
}

void LiveView::initCCTVStreams()
{
    qDebug() << "[LiveView] ========================================";
    qDebug() << "[LiveView] Initializing RTSP connections...";

    QString ip = "192.168.0.82";
    QString port = "8554";

    // WinId 확인
    for(int i = 0; i < 4; i++) {
        WId winId = cctvWidgets[i]->winId();
        qDebug() << "[LiveView] Channel" << (i+1) << "WinId:" << winId;
    }

    // 각 채널을 5초 간격으로 연결 (더 여유있게)
    for(int i = 0; i < 4; i++) {
        QString url = QString("rtsp://%1:%2/ch%3").arg(ip, port).arg(i+1);
        int delay = i * 5000; // 5초 간격

        qDebug() << "[LiveView] Channel" << (i+1) << "scheduled at" << delay << "ms";

        QTimer::singleShot(delay, this, [this, i, url]() {
            qDebug() << "";
            qDebug() << "[LiveView] ===== Connecting Channel" << (i+1) << "=====";
            qDebug() << "[LiveView] URL:" << url;
            cctvWidgets[i]->playUrl(url);
        });
    }

    qDebug() << "[LiveView] All channels scheduled";
    qDebug() << "[LiveView] ========================================";
}

void LiveView::setChannelVisible(int index, bool visible) {
    if (index < 4) {
        cctvWidgets[index]->setVisible(visible);
    } else if (index < 6) {
        sensorWidgets[index - 4]->setVisible(visible);
    }
}

bool LiveView::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::MouseButtonDblClick) {
        for (int i = 0; i < 4; i++) {
            if (obj == cctvWidgets[i]) {
                emit requestFullScreen(i);
                return true;
            }
        }
        for (int i = 0; i < 2; i++) {
            if (obj == sensorWidgets[i]) {
                emit requestFullScreen(i + 4);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}
