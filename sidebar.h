/**
 * @file sidebar.h
 * @brief 좌측 채널 목록(Dock) 헤더
 */
#ifndef SIDEBAR_H
#define SIDEBAR_H

#include <QDockWidget>
#include <QListWidget>

class Sidebar : public QDockWidget
{
    Q_OBJECT
public:
    explicit Sidebar(const QString &title, QWidget *parent = nullptr);

signals:
    // 채널 클릭 시 상태 변경 알림
    void channelStateChanged(int channelIndex, bool isVisible);

private:
    QListWidget *channelList;
    void setupList(); // 목록 초기화
    void addHeaderItem(QString title); // 그룹 제목 추가
    void addChannelItem(int index, QString name); // 채널 아이템 추가
    QString getChannelName(int index); // 채널 이름 반환 헬퍼

private slots:
    void onItemClicked(QListWidgetItem *item); // 클릭 이벤트 핸들러
};

#endif // SIDEBAR_H
