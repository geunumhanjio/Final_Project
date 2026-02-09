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

class Sidebar : public QDockWidget
{
    Q_OBJECT
public:
    explicit Sidebar(const QString &title, QWidget *parent = nullptr);

signals:
    void channelStateChanged(int channelIndex, bool isVisible);

private:
    QWidget *container;
    QLineEdit *searchBar;
    QListWidget *channelList;

    void setupUi();
    void setupList(); 
    void addHeaderItem(QString title, QString count); 
    void addChannelItem(int index, QString name, bool isLidar = false, bool useTextStatus = true);
    QString getChannelName(int index); 
    void filterChannels(const QString &text);

private slots:
    void onItemClicked(QListWidgetItem *item);
};

#endif // SIDEBAR_H
