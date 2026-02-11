#include "playbackview.h"
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo> // [New]

PlaybackView::PlaybackView(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(10);

    // Header
    QHBoxLayout *headerLayout = new QHBoxLayout();
    m_titleLabel = new QLabel("Recorded Videos", this);
    m_titleLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: white;");
    
    m_btnRefresh = new QPushButton("Refresh List", this);
    m_btnRefresh->setFixedSize(120, 40);
    m_btnRefresh->setStyleSheet(
        "QPushButton { background-color: #2563eb; color: white; border-radius: 6px; font-weight: bold; }"
        "QPushButton:hover { background-color: #1d4ed8; }"
    );
    
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_btnRefresh);

    // List Widget
    m_listWidget = new QListWidget(this);
    m_listWidget->setStyleSheet(
        "QListWidget { background-color: #1e293b; border-radius: 8px; border: 1px solid #334155; color: white; font-size: 16px; }"
        "QListWidget::item { padding: 10px; border-bottom: 1px solid #334155; }"
        "QListWidget::item:selected { background-color: #3b82f6; }"
        "QListWidget::item:hover { background-color: #334155; }"
    );

    // Progress Bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setAlignment(Qt::AlignCenter);
    m_progressBar->setVisible(false); // Initially hidden
    m_progressBar->setStyleSheet(
        "QProgressBar { border: 2px solid grey; border-radius: 5px; text-align: center; color: white; }"
        "QProgressBar::chunk { background-color: #05B8CC; width: 20px; }"
    );

    mainLayout->addLayout(headerLayout);
    mainLayout->addWidget(m_listWidget);
    mainLayout->addWidget(m_progressBar); // Add to layout

    connect(m_btnRefresh, &QPushButton::clicked, this, &PlaybackView::refreshRequested);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &PlaybackView::onItemClicked);
}

void PlaybackView::updateDownloadProgress(qint64 received, qint64 total)
{
    if (!m_progressBar->isVisible()) m_progressBar->setVisible(true);
    
    if (total > 0) {
        int percent = (received * 100) / total;
        m_progressBar->setValue(percent);
        m_progressBar->setFormat(QString("Downloading... %1%").arg(percent));
    }
    
    if (received >= total && total > 0) {
        m_progressBar->setVisible(false); // Hide upon completion
        m_progressBar->setValue(0);
    }
}

void PlaybackView::addLocalItem(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QString name = fileInfo.fileName();
    QString date = fileInfo.lastModified().toString("yyyy-MM-dd HH:mm:ss");
    
    QString displayText = QString("%1  |  %2 (Local)").arg(date, name);
    
    QListWidgetItem *item = new QListWidgetItem(displayText);
    item->setData(Qt::UserRole, name); // Store filename (relative for local check in MainWindow)
    item->setIcon(QIcon::fromTheme("video-x-generic"));
    
    // Add to top
    m_listWidget->insertItem(0, item);
}

void PlaybackView::updateList(const QJsonArray &recordings)
{
    m_listWidget->clear();
    
    for(const auto &val : recordings) {
        QJsonObject obj = val.toObject();
        QString name = obj["name"].toString();
        QString url = obj["url"].toString();
        QString date = obj["date"].toString();
        
        qDebug() << "[PlaybackView] Item:" << name << "URL:" << url; // Debug Log
        
        QString displayText = QString("%1  |  %2").arg(date, name);
        
        QListWidgetItem *item = new QListWidgetItem(displayText);
        item->setData(Qt::UserRole, name); // Use filename for correct download request
        item->setIcon(QIcon::fromTheme("video-x-generic"));
        
        m_listWidget->addItem(item);
    }
    
    if (m_listWidget->count() == 0) {
        m_listWidget->addItem("No recordings found.");
    }
}

void PlaybackView::onItemClicked(QListWidgetItem *item)
{
    QString url = item->data(Qt::UserRole).toString();
    if (!url.isEmpty()) {
        emit playRequested(url);
    }
}
