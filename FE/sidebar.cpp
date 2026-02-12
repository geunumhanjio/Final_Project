/**
 * @file sidebar.cpp
 * @brief Sidebar implementation
 */
#include "sidebar.h"
#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStackedLayout> // [New]
#include <QListWidget>
#include <QListWidgetItem>
#include <QLineEdit>

Sidebar::Sidebar(const QString &title, QWidget *parent)
    : QDockWidget(title, parent)
{
    this->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    
    // Hide default title bar
    QWidget* titleBarWidget = new QWidget(this);
    this->setTitleBarWidget(titleBarWidget);

    setupUi();
}

void Sidebar::setupUi()
{
    container = new QWidget(this);
    container->setObjectName("SidebarContainer");
    
    // Use Stacked Layout to switch between Live and Playback modes
    mainStack = new QStackedLayout(container);
    mainStack->setContentsMargins(0, 0, 0, 0);

    setupLiveUI();       // Index 0
    setupPlaybackUI();   // Index 1

    mainStack->addWidget(liveWidget);
    mainStack->addWidget(playbackWidget);

    this->setWidget(container);
}

void Sidebar::setMode(SidebarMode mode)
{
    if (mode == Live) {
        mainStack->setCurrentIndex(0);
        this->setWindowTitle("채널 목록");
    } else {
        mainStack->setCurrentIndex(1);
        this->setWindowTitle("녹화 목록");
    }
}

void Sidebar::setupLiveUI()
{
    liveWidget = new QWidget(container);
    QVBoxLayout *layout = new QVBoxLayout(liveWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 1. Search Bar Area
    QWidget *searchArea = new QWidget(liveWidget);
    searchArea->setObjectName("SidebarSearchArea");
    searchArea->setStyleSheet(""); 
    QVBoxLayout *searchLayout = new QVBoxLayout(searchArea);
    searchLayout->setContentsMargins(0, 12, 0, 12);
    
    searchBar = new QLineEdit(liveWidget);
    searchBar->setObjectName("SidebarSearchBar");
    searchBar->setPlaceholderText("Filter devices...");
    
    searchLayout->addWidget(searchBar);
    
    // 2. Channel List
    channelList = new QListWidget(liveWidget);
    channelList->setObjectName("SidebarList");
    channelList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setupList(); // Populates channelList using existing logic

    layout->addWidget(searchArea);
    layout->addWidget(channelList, 1); 

    connect(channelList, &QListWidget::itemClicked, this, &Sidebar::onItemClicked);
    connect(searchBar, &QLineEdit::textChanged, this, &Sidebar::filterChannels);
}

void Sidebar::setupPlaybackUI()
{
    playbackWidget = new QWidget(container);
    QVBoxLayout *layout = new QVBoxLayout(playbackWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // Header for Playback Mode
    QLabel *sidebarHeader = new QLabel("CATEGORIES", playbackWidget);
    sidebarHeader->setFixedHeight(50);
    sidebarHeader->setAlignment(Qt::AlignCenter);
    sidebarHeader->setObjectName("SidebarHeaderTitle"); // Reuse style if possible or add new
    sidebarHeader->setStyleSheet("color: #94A3B8; font-weight: bold; font-size: 14px; letter-spacing: 1px; border-bottom: 1px solid #334155;");
    layout->addWidget(sidebarHeader);

    categoryList = new QListWidget(playbackWidget);
    categoryList->setFocusPolicy(Qt::NoFocus);
    categoryList->setStyleSheet(
        "QListWidget { background-color: transparent; border: none; outline: none; }"
        "QListWidget::item { padding: 12px 16px; color: #CBD5E1; font-size: 14px; border-bottom: 1px solid #1E293B; }"
        "QListWidget::item:selected { background-color: #2563EB; color: white; font-weight: bold; border-left: 4px solid #60A5FA; }"
        "QListWidget::item:hover { background-color: #1E293B; }"
    );
    
    // Populate Categories (Same as PlaybackView logic)
    struct Category { int id; QString name; };
    QList<Category> categories = {
        {0, "📂  All Recordings"}, // [New]
        {1, "📹  CCTV 1 (Low)"},
        {2, "📹  CCTV 2 (Low)"},
        {3, "📹  CCTV 3 (Low)"},
        {4, "📹  CCTV 4 (Low)"},
        {5, "🎥  CCTV 1 (High)"},
        {6, "🎥  CCTV 2 (High)"},
        {7, "🎥  CCTV 3 (High)"},
        {8, "🎥  CCTV 4 (High)"},
        {9, "🚗  RC Car Camera"},
        {10, "📡  Lidar Map"}
    };

    for(const auto &cat : categories) {
        QListWidgetItem *item = new QListWidgetItem(cat.name);
        item->setData(Qt::UserRole, cat.id);
        categoryList->addItem(item);
    }

    layout->addWidget(categoryList);
    
    connect(categoryList, &QListWidget::itemClicked, this, &Sidebar::onCategoryClicked);
}

void Sidebar::onCategoryClicked(QListWidgetItem *item)
{
    if(!item) return;
    int catId = item->data(Qt::UserRole).toInt();
    emit categorySelected(catId);
}

void Sidebar::setupList()
{
    // Group 1: CCTV
    addHeaderItem("CCTV CHANNELS", "4");
    for(int i=0; i<4; i++) addChannelItem(i, getChannelName(i));

    // Group 2: Robot Sensors
    addHeaderItem("RC UNIT & SLAM", "2");
    addChannelItem(4, getChannelName(4), false, true); // Use Text for RC
    addChannelItem(5, getChannelName(5), true, true);  // Use Text for Lidar
}

void Sidebar::addHeaderItem(QString title, QString count) {
    QListWidgetItem *item = new QListWidgetItem();
    item->setFlags(Qt::NoItemFlags);
    
    // Create custom widget for header
    QWidget *headerWidget = new QWidget();
    QHBoxLayout *hLayout = new QHBoxLayout(headerWidget);
    hLayout->setContentsMargins(12, 16, 12, 4);
    
    QLabel *titleLabel = new QLabel(title);
    titleLabel->setObjectName("SidebarHeaderTitle");
    
    QLabel *countLabel = new QLabel(count);
    countLabel->setObjectName("SidebarHeaderCount");
    
    hLayout->addWidget(titleLabel);
    hLayout->addStretch();
    hLayout->addWidget(countLabel);
    
    item->setSizeHint(headerWidget->sizeHint());
    channelList->addItem(item);
    channelList->setItemWidget(item, headerWidget);
}

void Sidebar::addChannelItem(int index, QString name, bool isLidar, bool useTextStatus) {
    QListWidgetItem *item = new QListWidgetItem();
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setData(Qt::UserRole, 1); // State: ON
    item->setData(Qt::UserRole + 1, index);

    // Custom Widget
    QWidget *itemWidget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(itemWidget);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(10);
    
    // Icon
    QLabel *icon = new QLabel(isLidar ? "📡" : "📹"); // Unicode icons
    icon->setObjectName("SidebarItemIcon");
    
    // Text
    QLabel *text = new QLabel(name);
    text->setObjectName("SidebarItemText");

    layout->addWidget(icon);
    layout->addWidget(text);
    layout->addStretch();

    if (useTextStatus) {
        // Status Label (ON/OFF)
        QLabel *statusLabel = new QLabel("ON");
        statusLabel->setObjectName("SidebarItemStatus");
        statusLabel->setAlignment(Qt::AlignCenter);
        // Style handled in QSS or dynamic update
        statusLabel->setStyleSheet("color: #22c55e; font-weight: bold; font-size: 11px;"); 
        layout->addWidget(statusLabel);
    } else {
        // Status Dot
        QLabel *dot = new QLabel("●");
        dot->setObjectName("SidebarItemDot");
        dot->setStyleSheet("color: #22c55e; font-size: 10px;");
        layout->addWidget(dot);
    }
    
    item->setSizeHint(itemWidget->sizeHint());
    channelList->addItem(item);
    channelList->setItemWidget(item, itemWidget);
}

QString Sidebar::getChannelName(int index) {
    if (index < 4) return QString("Channel 0%1 - Camera").arg(index + 1);
    if (index == 4) return "RC Front Cam";
    return "Lidar Map Stream";
}

void Sidebar::onItemClicked(QListWidgetItem *item) {
    // Basic toggle logic would go here.
    // For now, just emit the signal.
    int idx = item->data(Qt::UserRole + 1).toInt();
    int state = item->data(Qt::UserRole).toInt();
    
    // Toggle state
    state = !state;
    item->setData(Qt::UserRole, state);
    
    emit channelStateChanged(idx, state); // Emit signal first

    // Update visual (need to access the custom widget's labels)
    QWidget *widget = channelList->itemWidget(item);
    if(widget) {
        QList<QLabel*> labels = widget->findChildren<QLabel*>();
        // Check for specific labels
        QLabel *statusLabel = widget->findChild<QLabel*>("SidebarItemStatus");
        if (statusLabel) {
            if (state) {
                statusLabel->setText("ON");
                statusLabel->setStyleSheet("color: #22c55e; font-weight: bold; font-size: 11px;");
            } else {
                statusLabel->setText("OFF");
                statusLabel->setStyleSheet("color: #ef4444; font-weight: bold; font-size: 11px;");
            }
            return;
        }

        QLabel *dotLabel = widget->findChild<QLabel*>("SidebarItemDot");
        if (dotLabel) {
            dotLabel->setStyleSheet(state ? "color: #22c55e; font-size: 10px;" : "color: #ef4444; font-size: 10px;");
        }
    }
}

void Sidebar::filterChannels(const QString &text)
{
    // Simple filter logic
    for(int i = 0; i < channelList->count(); ++i)
    {
        QListWidgetItem *item = channelList->item(i);
        QWidget *widget = channelList->itemWidget(item);
        if(!widget) continue; // Header items might fallback
        
        // Skip headers
        if(item->flags() == Qt::NoItemFlags) continue;
        
        QList<QLabel*> labels = widget->findChildren<QLabel*>();
        if(labels.size() >= 2) {
             if(labels[1]->text().contains(text, Qt::CaseInsensitive))
                 item->setHidden(false);
             else
                 item->setHidden(true);
        }
    }
}
