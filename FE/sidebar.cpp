/**
 * @file sidebar.cpp
 * @brief Sidebar implementation
 */
#include "sidebar.h"
#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
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
    
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 1. Search Bar Area
    QWidget *searchArea = new QWidget(container);
    searchArea->setObjectName("SidebarSearchArea");
    searchArea->setStyleSheet(""); // Clear just in case
    QVBoxLayout *searchLayout = new QVBoxLayout(searchArea);
    searchLayout->setContentsMargins(0, 12, 0, 12); // Padding moved to layout or handled in QSS if possible, but padding usually layout.
    
    searchBar = new QLineEdit(container);
    searchBar->setObjectName("SidebarSearchBar");
    searchBar->setPlaceholderText("Filter devices...");
    
    searchLayout->addWidget(searchBar);
    
    // 2. Channel List
    channelList = new QListWidget(container);
    channelList->setObjectName("SidebarList");
    channelList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setupList();

    layout->addWidget(searchArea);
    layout->addWidget(channelList, 1); // Stretch factor 1
    
    // 3. Bottom Stats (Optional, matching design)
    // For simplicity, omitting bottom stats or adding a spacer is fine. 
    // The design has CPU Load / Network. We can add a simple placeholder if needed, 
    // but the task is mainly about appearance of the list.

    this->setWidget(container);

    connect(channelList, &QListWidget::itemClicked, this, &Sidebar::onItemClicked);
    connect(searchBar, &QLineEdit::textChanged, this, &Sidebar::filterChannels);
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
