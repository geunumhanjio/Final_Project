/**
 * @file topbar.cpp
 * @brief 상단 버튼 배치 및 스타일 설정
 */
#include "topbar.h"

TopBar::TopBar(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    this->setFixedHeight(60); // 높이 고정
    this->setStyleSheet("background-color: #333333;");

    layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 10, 0);

    // 햄버거 메뉴 버튼
    btnToggle = new QPushButton("≡", this);
    btnToggle->setFixedSize(50, 50);
    btnToggle->setStyleSheet("QPushButton { color: white; background: transparent; font-size: 30px; border: none; } QPushButton:hover { color: #FFA500; }");

    // 네비게이션 버튼들
    btnLive = new QPushButton("라이브 관제", this);
    btnPlayback = new QPushButton("영상 기록", this);
    btnSettings = new QPushButton("설정", this);

    QString btnStyle = "QPushButton { color: white; background: transparent; font-size: 16px; font-weight: bold; border: none; padding: 10px; } QPushButton:hover { color: #FFA500; }";
    btnLive->setStyleSheet(btnStyle);
    btnPlayback->setStyleSheet(btnStyle);
    btnSettings->setStyleSheet(btnStyle);

    // 레이아웃 배치
    layout->addWidget(btnToggle);
    layout->addStretch(1); // 왼쪽 공백
    layout->addWidget(btnLive);
    layout->addWidget(btnPlayback);
    layout->addWidget(btnSettings);
    layout->addStretch(1); // 오른쪽 공백 (중앙 정렬 효과)

    // 시그널 연결
    connect(btnToggle, &QPushButton::clicked, this, &TopBar::sidebarToggled);
    connect(btnLive, &QPushButton::clicked, [=](){ emit modeChanged(0); });
    connect(btnPlayback, &QPushButton::clicked, [=](){ emit modeChanged(1); });
    connect(btnSettings, &QPushButton::clicked, [=](){ emit modeChanged(2); });
}
