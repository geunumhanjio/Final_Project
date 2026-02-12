/**
 * @file sidebar.h
 * @brief Left sidebar header
 */
#ifndef SIDEBAR_H
#define SIDEBAR_H

#include <QDockWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QVBoxLayout>

class QStackedLayout; // [Fixed] 전방 선언

class Sidebar : public QDockWidget
{
    Q_OBJECT

public:
    enum SidebarMode { Live, Playback };
    Q_ENUM(SidebarMode)

    explicit Sidebar(const QString &title, QWidget *parent = nullptr); // [Fixed] 생성자 복구

public slots:
    void setMode(SidebarMode mode); // [New] 모드 설정 슬롯

signals:
    void channelStateChanged(int channelIndex, bool isVisible);
    void categorySelected(int categoryId); // [New] 카테고리 선택 시그널

private:
    QWidget *container;
    // Layouts & Widgets for switching
    QStackedLayout *mainStack; // [New]
    QWidget *liveWidget;       // [New] 기존 채널 목록 컨테이너
    QWidget *playbackWidget;   // [New] 재생 카테고리 목록 컨테이너

    // Live Mode Components
    QLineEdit *searchBar;
    QListWidget *channelList;

    // Playback Mode Components
    QListWidget *categoryList; // [New]

    void setupUi();
    void setupLiveUI();       // [New] 기존 setupUi 로직 이동
    void setupPlaybackUI();   // [New] 재생 모드 UI 초기화
    
    void setupList(); 
    void addHeaderItem(QString title, QString count); 
    void addChannelItem(int index, QString name, bool isLidar = false, bool useTextStatus = true);
    QString getChannelName(int index); 
    void filterChannels(const QString &text);

private slots:
    void onItemClicked(QListWidgetItem *item);
    void onCategoryClicked(QListWidgetItem *item); // [New]
};

#endif // SIDEBAR_H
