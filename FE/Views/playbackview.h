#ifndef PLAYBACKVIEW_H
#define PLAYBACKVIEW_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QJsonObject>
#include <QProgressBar> // [New]

class PlaybackView : public QWidget
{
    Q_OBJECT
public:
    explicit PlaybackView(QWidget *parent = nullptr);

    // Update the list with data from server
    void updateList(const QJsonArray &recordings);
    void updateDownloadProgress(qint64 received, qint64 total);
    void addLocalItem(const QString &filePath); // [New]
    void filterRecordings(int categoryId); // [New] 필터링 함수 (Public으로 변경)

signals:
    void refreshRequested();
    void playRequested(const QString &url);

private slots:
    void onItemClicked(QListWidgetItem *item);

private:
    // UI Components
    QListWidget *m_listWidget;   // 파일 목록
    QPushButton *m_btnRefresh;
    QLabel *m_titleLabel;
    QProgressBar *m_progressBar;

    // Data
    QList<QJsonObject> m_allRecordings; // [New] 전체 녹화 데이터 저장
    int m_currentCategory;              // [New] 현재 선택된 카테고리 ID (1~10)
};

#endif // PLAYBACKVIEW_H
