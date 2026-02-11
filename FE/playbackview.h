#ifndef PLAYBACKVIEW_H
#define PLAYBACKVIEW_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
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
    void addLocalItem(const QString &filePath); // [New] // [New]

signals:
    void refreshRequested();
    void playRequested(const QString &url);

private slots:
    void onItemClicked(QListWidgetItem *item);

private:
    QListWidget *m_listWidget;
    QPushButton *m_btnRefresh;
    QLabel *m_titleLabel;
    QProgressBar *m_progressBar; // [New]
};

#endif // PLAYBACKVIEW_H
