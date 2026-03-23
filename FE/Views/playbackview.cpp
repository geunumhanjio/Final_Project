#include "playbackview.h"
#include "channelcatalog.h"
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QSplitter>

PlaybackView::PlaybackView(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(10);

    // Header
    QHBoxLayout *headerLayout = new QHBoxLayout();
    m_titleLabel = new QLabel("Videos - Select Category", this);
    m_titleLabel->setObjectName("PlaybackTitle"); // [New]
    // [Mod] Removed inline style
    
    m_btnRefresh = new QPushButton("Refresh", this);
    m_btnRefresh->setFixedSize(100, 36);
    // Refresh button style kept for now or can be moved too, but user focused on Title/List.
    // I'll leave the button inline for now as it wasn't mentioned, but ObjectName is better.
    m_btnRefresh->setObjectName("PlaybackRefreshBtn");
    m_btnRefresh->setStyleSheet(
        "QPushButton { background-color: #2563eb; color: white; border-radius: 6px; font-weight: bold; }"
        "QPushButton:hover { background-color: #1d4ed8; }"
    );
    
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_btnRefresh);

    // File List
    m_listWidget = new QListWidget(this);
    m_listWidget->setObjectName("PlaybackList"); // [New]
    // [Mod] Removed inline style to use QSS (will enforce Dark style in both themes)

    // Progress Bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setAlignment(Qt::AlignCenter);
    m_progressBar->setVisible(false);
    m_progressBar->setStyleSheet(
        "QProgressBar { border: 2px solid grey; border-radius: 5px; text-align: center; color: white; }"
        "QProgressBar::chunk { background-color: #05B8CC; width: 20px; }"
    );

    mainLayout->addLayout(headerLayout);
    mainLayout->addWidget(m_listWidget);
    mainLayout->addWidget(m_progressBar);

    // Connections
    connect(m_btnRefresh, &QPushButton::clicked, this, &PlaybackView::refreshRequested);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &PlaybackView::onItemClicked);
    
    // Default Selection (CCTV 1 Low) -> All (0)
    m_currentCategory = 0;
}



void PlaybackView::filterRecordings(int categoryId)
{
    m_currentCategory = categoryId;
    m_listWidget->clear();
    m_titleLabel->setText(QStringLiteral("Videos - %1").arg(ChannelCatalog::playbackCategoryName(categoryId)));
    
    for(const QJsonObject &obj : m_allRecordings) {
         const QString name = obj["name"].toString();
         const int id = ChannelCatalog::parseRecordingChannelId(name);
         
         if (categoryId == 0 || id == categoryId) {
             QString displayText = QString("[%1] %2").arg(obj["dateFormatted"].toString(), name);
             
             QListWidgetItem *item = new QListWidgetItem(displayText);
             item->setData(Qt::UserRole, name);
             item->setIcon(QIcon::fromTheme("video-x-generic"));
             m_listWidget->addItem(item);
         }
    }
    
    if (m_listWidget->count() == 0) {
        m_listWidget->addItem("No recordings found for this category.");
    }
}

void PlaybackView::updateList(const QJsonArray &recordings)
{
    m_allRecordings.clear();
    
    for(const auto &val : recordings) {
        QJsonObject obj = val.toObject();
        // Add formatted date for display
        QString rawDate = obj["date"].toString(); 
        // Assuming rawDate is ISO or similar, keep simple for now or parsing if needed
        obj["dateFormatted"] = rawDate; 
        
        m_allRecordings.append(obj);
    }
    
    qDebug() << "[PlaybackView] Updated list with" << m_allRecordings.size() << "items.";
    filterRecordings(m_currentCategory);
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
    // Local additions should just trigger a refresh to get full list and sort correctly
    // But specific request to add local item:
    // We construct a fake object and add to m_allRecordings, then filter
    
    QFileInfo fileInfo(filePath);
    QString name = fileInfo.fileName();
    QString date = fileInfo.lastModified().toString("yyyy-MM-dd HH:mm:ss");
    
    QJsonObject obj;
    obj["name"] = name;
    obj["url"] = ""; // Local
    obj["date"] = date;
    obj["dateFormatted"] = date;
    
    // Check if duplicate (by name)
    bool exists = false;
    for(const auto &existing : m_allRecordings) {
        if(existing["name"].toString() == name) { exists = true; break; }
    }
    
    if (!exists) {
        m_allRecordings.prepend(obj); // Add new local file to top
        filterRecordings(m_currentCategory);
    }
}

void PlaybackView::onItemClicked(QListWidgetItem *item)
{
    QString name = item->data(Qt::UserRole).toString();
    if (name.isEmpty() || name.contains("No recordings")) return;
    
    emit playRequested(name);
}
