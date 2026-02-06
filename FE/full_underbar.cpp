#include "full_underbar.h"

FullUnderBar::FullUnderBar(QWidget *parent) : QWidget(parent)
{
    this->setFixedHeight(60);
    this->setStyleSheet("background-color: #222222; border-top: 1px solid #444;");

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setSpacing(20);
    layout->setContentsMargins(20, 10, 20, 10);

    QString btnStyle =
        "QPushButton { "
        "   background-color: #444; color: white; font-size: 16px; "
        "   padding: 10px 20px; border-radius: 5px; "
        "}"
        "QPushButton:hover { background-color: #555; }"
        "QPushButton:checked { background-color: #007ACC; border: 1px solid #0099FF; }";

    // 1. 확대 버튼
    btnZoomIn = new QPushButton("+ 확대(중앙)", this);
    btnZoomIn->setStyleSheet(btnStyle);
    connect(btnZoomIn, &QPushButton::clicked, this, &FullUnderBar::reqZoomIn);

    // 2. 축소 버튼
    btnZoomOut = new QPushButton("- 축소(단계)", this);
    btnZoomOut->setStyleSheet(btnStyle);
    connect(btnZoomOut, &QPushButton::clicked, this, &FullUnderBar::reqZoomOut);

    // 3. 네모 확대 버튼
    btnRectZoom = new QPushButton("[ ] 네모 확대", this);
    btnRectZoom->setCheckable(true);
    btnRectZoom->setStyleSheet(btnStyle);
    connect(btnRectZoom, &QPushButton::toggled, this, &FullUnderBar::reqRectZoom);

    // 4. [신규] 초기화 버튼
    btnResetZoom = new QPushButton("↺ 확대 해제 (초기화)", this);
    btnResetZoom->setStyleSheet(btnStyle + "QPushButton { background-color: #0055aa; } QPushButton:hover { background-color: #0066cc; }");
    connect(btnResetZoom, &QPushButton::clicked, this, &FullUnderBar::reqResetZoom);

    layout->addStretch();
    layout->addWidget(btnZoomIn);
    layout->addWidget(btnZoomOut);
    layout->addWidget(btnRectZoom);
    layout->addWidget(btnResetZoom); // 우측 배치
    layout->addStretch();
}

void FullUnderBar::setRectButtonMode(int state)
{
    btnRectZoom->blockSignals(true);
    switch (state) {
    case 0:
        btnRectZoom->setText("[ ] 네모 확대");
        btnRectZoom->setChecked(false);
        break;
    case 1:
        btnRectZoom->setText("그리기 취소");
        btnRectZoom->setChecked(true);
        break;
    case 2:
        // 확대된 상태여도 다시 확대할 수 있도록 네모 버튼 원복
        btnRectZoom->setText("[ ] 네모 확대");
        btnRectZoom->setChecked(false);
        break;
    }
    btnRectZoom->blockSignals(false);
}
