/**
 * @file fullscreenview.cpp
 * @brief 전체 화면 UI 및 닫기 로직
 */
#include "fullscreenview.h"
#include <QEvent>

FullScreenView::FullScreenView(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    this->setStyleSheet("background-color: black;");

    QGridLayout *layout = new QGridLayout(this);
    layout->setContentsMargins(0,0,0,0);

    // 영상이 표시될 라벨 (추후 VideoWidget으로 교체 가능)
    screenLabel = new QLabel("FULL SCREEN", this);
    screenLabel->setAlignment(Qt::AlignCenter);
    screenLabel->setStyleSheet("QLabel { color: white; font-size: 40px; background-color: black; }");
    screenLabel->installEventFilter(this); // 더블클릭 감지

    // 닫기 버튼 (우상단)
    btnClose = new QPushButton("X", this);
    btnClose->setFixedSize(50, 50);
    btnClose->setStyleSheet("QPushButton { background-color: red; color: white; font-weight: bold; font-size: 20px; border: none; } QPushButton:hover { background-color: #FF5555; }");

    // 배치
    layout->addWidget(screenLabel, 0, 0);
    layout->addWidget(btnClose, 0, 0, Qt::AlignTop | Qt::AlignRight);

    connect(btnClose, &QPushButton::clicked, this, &FullScreenView::closeRequested);
}

void FullScreenView::setContent(int index) {
    screenLabel->setText(QString("%1\n(Live Full Screen)").arg(getChannelName(index)));
}

QString FullScreenView::getChannelName(int index) {
    if (index < 4) return QString("CCTV Camera %1").arg(index + 1);
    if (index == 4) return "RC Car Camera";
    return "LiDAR SLAM Map";
}

bool FullScreenView::eventFilter(QObject *obj, QEvent *event) {
    // 화면 더블 클릭 시 닫기
    if (event->type() == QEvent::MouseButtonDblClick && obj == screenLabel) {
        emit closeRequested();
        return true;
    }
    return QWidget::eventFilter(obj, event);
}
