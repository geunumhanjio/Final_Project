/**
 * @file sidebar.cpp
 * @brief 리스트 위젯 관리 및 클릭 이벤트 처리
 */
#include "sidebar.h"
#include <QWidget>

Sidebar::Sidebar(const QString &title, QWidget *parent)
    : QDockWidget(title, parent)
{
    this->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    // 기본 타이틀바 숨기기 (깔끔한 UI 위해)
    QWidget* titleBarWidget = new QWidget(this);
    this->setTitleBarWidget(titleBarWidget);

    channelList = new QListWidget(this);
    channelList->setStyleSheet("QListWidget { background-color: #222; outline: none; border: none; }"
                               "QListWidget::item { padding: 12px; color: white; border-bottom: 1px solid #444; }"
                               "QListWidget::item:selected { background-color: #444; color: #FFA500; }");

    setupList();
    this->setWidget(channelList);

    // 아이템 클릭 시그널 연결
    connect(channelList, &QListWidget::itemClicked, this, &Sidebar::onItemClicked);
}

void Sidebar::setupList()
{
    // 그룹 1: CCTV
    addHeaderItem("=== CCTV CAMERAS ===");
    for(int i=0; i<4; i++) addChannelItem(i, getChannelName(i));

    // 그룹 2: 로봇 센서
    addHeaderItem("=== ROBOT SENSORS ===");
    addChannelItem(4, getChannelName(4));
    addChannelItem(5, getChannelName(5));
}

void Sidebar::addHeaderItem(QString title) {
    QListWidgetItem *header = new QListWidgetItem(title);
    header->setFlags(Qt::NoItemFlags); // 클릭 불가
    header->setBackground(QColor("#111"));
    header->setForeground(QBrush(QColor("#FFA500")));
    header->setTextAlignment(Qt::AlignCenter);
    channelList->addItem(header);
}

void Sidebar::addChannelItem(int index, QString name) {
    QListWidgetItem *item = new QListWidgetItem(QString("%1  [ON]").arg(name));
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    // [중요] 아이템 내부에 데이터 저장
    item->setData(Qt::UserRole, 1);       // 상태: 1=ON, 0=OFF
    item->setData(Qt::UserRole + 1, index); // 실제 채널 ID (0~5)

    channelList->addItem(item);
}

QString Sidebar::getChannelName(int index) {
    if (index < 4) return QString("CCTV Camera %1").arg(index + 1);
    if (index == 4) return "RC Car Camera";
    return "LiDAR SLAM Map";
}

void Sidebar::onItemClicked(QListWidgetItem *item) {
    // 헤더 클릭 시 무시
    if (!(item->flags() & Qt::ItemIsSelectable)) return;

    // 저장된 데이터 불러오기
    int idx = item->data(Qt::UserRole + 1).toInt();
    int state = item->data(Qt::UserRole).toInt();
    QString name = getChannelName(idx);

    if (state == 1) { // 현재 ON이면 -> OFF로 변경
        item->setData(Qt::UserRole, 0);
        item->setText(QString("%1  [OFF]").arg(name));
        item->setForeground(QBrush(QColor(100, 100, 100))); // 회색
        emit channelStateChanged(idx, false);
    } else { // 현재 OFF면 -> ON으로 변경
        item->setData(Qt::UserRole, 1);
        item->setText(QString("%1  [ON]").arg(name));
        item->setForeground(QBrush(QColor(255, 255, 255))); // 흰색
        emit channelStateChanged(idx, true);
    }
}
