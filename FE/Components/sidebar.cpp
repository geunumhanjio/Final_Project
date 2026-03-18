/**
 * @file sidebar.cpp
 * @brief Sidebar implementation
 */
#include "sidebar.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QStackedLayout>
#include <QWidget>

Sidebar::Sidebar(const QString &title, QWidget *parent)
    : QDockWidget(title, parent)
{
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    QWidget *titleBarWidget = new QWidget(this);
    setTitleBarWidget(titleBarWidget);

    setupUi();
}

Sidebar::RobotMode Sidebar::currentRobotMode() const
{
    if (controlModeButton && controlModeButton->isChecked()) {
        return ControlMode;
    }
    if (autoModeButton && autoModeButton->isChecked()) {
        return AutoMode;
    }
    return ManualMode;
}

bool Sidebar::isControlButtonActive() const
{
    return activateControlButton && activateControlButton->isChecked();
}

void Sidebar::setControlButtonActive(bool active)
{
    if (!activateControlButton) {
        return;
    }

    const QString nextText = active ? QStringLiteral("Stop Control") : QStringLiteral("Start Control");
    const bool stateChanged = (activateControlButton->isChecked() != active);
    const bool textChanged = (activateControlButton->text() != nextText);

    if (!stateChanged && !textChanged) {
        return;
    }

    activateControlButton->blockSignals(true);
    activateControlButton->setChecked(active);
    activateControlButton->setText(nextText);
    activateControlButton->blockSignals(false);

    emit controlButtonToggled(active);
}

void Sidebar::setRobotMode(RobotMode mode)
{
    const bool modeChanged = (currentRobotMode() != mode);
    applyRobotModeSelection(mode, modeChanged);
}

void Sidebar::setupUi()
{
    container = new QWidget(this);
    container->setObjectName("SidebarContainer");

    mainStack = new QStackedLayout(container);
    mainStack->setContentsMargins(0, 0, 0, 0);

    setupLiveUI();
    setupPlaybackUI();

    mainStack->addWidget(liveWidget);
    mainStack->addWidget(playbackWidget);

    setWidget(container);
}

void Sidebar::setMode(SidebarMode mode)
{
    if (mode == Live) {
        mainStack->setCurrentIndex(0);
        setWindowTitle("Channel List");
    } else {
        mainStack->setCurrentIndex(1);
        setWindowTitle("Recordings");
    }
}

void Sidebar::setupLiveUI()
{
    liveWidget = new QWidget(container);
    QVBoxLayout *layout = new QVBoxLayout(liveWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QWidget *searchArea = new QWidget(liveWidget);
    searchArea->setObjectName("SidebarSearchArea");
    QVBoxLayout *searchLayout = new QVBoxLayout(searchArea);
    searchLayout->setContentsMargins(0, 12, 0, 12);

    searchBar = new QLineEdit(liveWidget);
    searchBar->setObjectName("SidebarSearchBar");
    searchBar->setPlaceholderText("Filter devices...");
    searchLayout->addWidget(searchBar);

    channelList = new QListWidget(liveWidget);
    channelList->setObjectName("SidebarList");
    channelList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setupList();

    QWidget *modePanel = new QWidget(liveWidget);
    modePanel->setObjectName("SidebarModePanel");
    modePanel->setMinimumHeight(240);
    QVBoxLayout *modeLayout = new QVBoxLayout(modePanel);
    modeLayout->setContentsMargins(12, 12, 12, 12);
    modeLayout->setSpacing(10);

    QLabel *modeTitle = new QLabel("ROBOT MODE", modePanel);
    modeTitle->setObjectName("SidebarModeTitle");
    modeLayout->addWidget(modeTitle);

    manualModeButton = new QRadioButton("Manual Mode", modePanel);
    autoModeButton = new QRadioButton("Auto Mode", modePanel);
    controlModeButton = new QRadioButton("Control Mode", modePanel);
    manualModeButton->setObjectName("SidebarModeOption");
    autoModeButton->setObjectName("SidebarModeOption");
    controlModeButton->setObjectName("SidebarModeOption");

    modeButtonGroup = new QButtonGroup(modePanel);
    modeButtonGroup->setExclusive(true);
    modeButtonGroup->addButton(manualModeButton, ManualMode);
    modeButtonGroup->addButton(autoModeButton, AutoMode);
    modeButtonGroup->addButton(controlModeButton, ControlMode);
    manualModeButton->setChecked(true);

    modeLayout->addWidget(manualModeButton);
    modeLayout->addWidget(autoModeButton);
    modeLayout->addWidget(controlModeButton);

    activateControlButton = new QPushButton("Start Control", modePanel);
    activateControlButton->setObjectName("SidebarControlButton");
    activateControlButton->setMinimumHeight(40);
    activateControlButton->setCheckable(true);
    activateControlButton->setEnabled(false);
    modeLayout->addWidget(activateControlButton);

    emergencyStopButton = new QPushButton(QStringLiteral("즉시 정지"), modePanel);
    emergencyStopButton->setObjectName("SidebarEmergencyStopButton");
    emergencyStopButton->setMinimumHeight(40);
    emergencyStopButton->setEnabled(false);
    modeLayout->addWidget(emergencyStopButton);

    layout->addWidget(searchArea);
    layout->addWidget(channelList, 1);
    layout->addWidget(modePanel, 0);

    connect(channelList, &QListWidget::itemClicked, this, &Sidebar::onItemClicked);
    connect(searchBar, &QLineEdit::textChanged, this, &Sidebar::filterChannels);
    connect(modeButtonGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int id) {
        applyRobotModeSelection(static_cast<RobotMode>(id), true);
    });
    connect(activateControlButton, &QPushButton::toggled, this, [this](bool checked) {
        activateControlButton->setText(checked ? "Stop Control" : "Start Control");
        emit controlButtonToggled(checked);
    });
    connect(emergencyStopButton, &QPushButton::clicked, this, &Sidebar::emergencyStopRequested);
}

void Sidebar::applyRobotModeSelection(RobotMode mode, bool emitModeSignal)
{
    if (modeButtonGroup) {
        if (QAbstractButton *button = modeButtonGroup->button(mode)) {
            if (!button->isChecked()) {
                const QSignalBlocker blocker(modeButtonGroup);
                button->setChecked(true);
            }
        }
    }

    const bool controlModeSelected = (mode == ControlMode);
    if (activateControlButton) {
        activateControlButton->setEnabled(controlModeSelected);
    }
    if (emergencyStopButton) {
        emergencyStopButton->setEnabled(controlModeSelected);
    }

    if (emitModeSignal) {
        emit robotModeChanged(mode);
    }

    setControlButtonActive(controlModeSelected);
}

void Sidebar::setupPlaybackUI()
{
    playbackWidget = new QWidget(container);
    QVBoxLayout *layout = new QVBoxLayout(playbackWidget);
    layout->setContentsMargins(0, 0, 0, 0);

    QLabel *sidebarHeader = new QLabel("CATEGORIES", playbackWidget);
    sidebarHeader->setFixedHeight(50);
    sidebarHeader->setAlignment(Qt::AlignCenter);
    sidebarHeader->setObjectName("PlaybackHeaderTitle");
    layout->addWidget(sidebarHeader);

    categoryList = new QListWidget(playbackWidget);
    categoryList->setObjectName("CategoryList");
    categoryList->setFocusPolicy(Qt::NoFocus);

    struct Category {
        int id;
        QString name;
    };
    const QList<Category> categories = {
        {0, "All Recordings"},
        {1, "CCTV 1 (Low)"},
        {2, "CCTV 2 (Low)"},
        {3, "CCTV 3 (Low)"},
        {4, "CCTV 4 (Low)"},
        {5, "CCTV 1 (High)"},
        {6, "CCTV 2 (High)"},
        {7, "CCTV 3 (High)"},
        {8, "CCTV 4 (High)"},
        {9, "RC Car Camera"},
        {10, "Lidar Map"}
    };

    for (const auto &cat : categories) {
        QListWidgetItem *item = new QListWidgetItem(cat.name);
        item->setData(Qt::UserRole, cat.id);
        categoryList->addItem(item);
    }

    layout->addWidget(categoryList);
    connect(categoryList, &QListWidget::itemClicked, this, &Sidebar::onCategoryClicked);
}

void Sidebar::onCategoryClicked(QListWidgetItem *item)
{
    if (!item) {
        return;
    }

    emit categorySelected(item->data(Qt::UserRole).toInt());
}

void Sidebar::setupList()
{
    addHeaderItem("CCTV CHANNELS", "4");
    for (int i = 0; i < 4; ++i) {
        addChannelItem(i, getChannelName(i));
    }

    addHeaderItem("RC UNIT & SLAM", "2");
    addChannelItem(4, getChannelName(4), false, true);
    addChannelItem(5, getChannelName(5), true, true);
}

void Sidebar::addHeaderItem(QString title, QString count)
{
    QListWidgetItem *item = new QListWidgetItem();
    item->setFlags(Qt::NoItemFlags);

    QWidget *headerWidget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(headerWidget);
    layout->setContentsMargins(12, 16, 12, 4);

    QLabel *titleLabel = new QLabel(title);
    titleLabel->setObjectName("SidebarHeaderTitle");

    QLabel *countLabel = new QLabel(count);
    countLabel->setObjectName("SidebarHeaderCount");

    layout->addWidget(titleLabel);
    layout->addStretch();
    layout->addWidget(countLabel);

    item->setSizeHint(headerWidget->sizeHint());
    channelList->addItem(item);
    channelList->setItemWidget(item, headerWidget);
}

void Sidebar::addChannelItem(int index, QString name, bool isLidar, bool useTextStatus)
{
    QListWidgetItem *item = new QListWidgetItem();
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setData(Qt::UserRole, 1);
    item->setData(Qt::UserRole + 1, index);

    QWidget *itemWidget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(itemWidget);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(10);

    QLabel *icon = new QLabel(isLidar ? "MAP" : "CAM");
    icon->setObjectName("SidebarItemIcon");

    QLabel *text = new QLabel(name);
    text->setObjectName("SidebarItemText");

    layout->addWidget(icon);
    layout->addWidget(text);
    layout->addStretch();

    if (useTextStatus) {
        QLabel *statusLabel = new QLabel("ON");
        statusLabel->setObjectName("SidebarItemStatus");
        statusLabel->setAlignment(Qt::AlignCenter);
        statusLabel->setStyleSheet("color: #22c55e; font-weight: bold; font-size: 11px;");
        layout->addWidget(statusLabel);
    } else {
        QLabel *dot = new QLabel("o");
        dot->setObjectName("SidebarItemDot");
        dot->setStyleSheet("color: #22c55e; font-size: 10px;");
        layout->addWidget(dot);
    }

    item->setSizeHint(itemWidget->sizeHint());
    channelList->addItem(item);
    channelList->setItemWidget(item, itemWidget);
}

QString Sidebar::getChannelName(int index)
{
    if (index < 4) {
        return QString("Channel 0%1 - Camera").arg(index + 1);
    }
    if (index == 4) {
        return "RC Front Cam";
    }
    return "Lidar Map Stream";
}

void Sidebar::onItemClicked(QListWidgetItem *item)
{
    if (!item) {
        return;
    }

    const int idx = item->data(Qt::UserRole + 1).toInt();
    int state = item->data(Qt::UserRole).toInt();
    state = !state;
    item->setData(Qt::UserRole, state);

    emit channelStateChanged(idx, state);

    QWidget *widget = channelList->itemWidget(item);
    if (!widget) {
        return;
    }

    if (QLabel *statusLabel = widget->findChild<QLabel *>("SidebarItemStatus")) {
        if (state) {
            statusLabel->setText("ON");
            statusLabel->setStyleSheet("color: #22c55e; font-weight: bold; font-size: 11px;");
        } else {
            statusLabel->setText("OFF");
            statusLabel->setStyleSheet("color: #ef4444; font-weight: bold; font-size: 11px;");
        }
        return;
    }

    if (QLabel *dotLabel = widget->findChild<QLabel *>("SidebarItemDot")) {
        dotLabel->setStyleSheet(state ? "color: #22c55e; font-size: 10px;"
                                      : "color: #ef4444; font-size: 10px;");
    }
}

void Sidebar::filterChannels(const QString &text)
{
    for (int i = 0; i < channelList->count(); ++i) {
        QListWidgetItem *item = channelList->item(i);
        QWidget *widget = channelList->itemWidget(item);
        if (!widget || item->flags() == Qt::NoItemFlags) {
            continue;
        }

        const QList<QLabel *> labels = widget->findChildren<QLabel *>();
        if (labels.size() < 2) {
            continue;
        }

        item->setHidden(!labels.at(1)->text().contains(text, Qt::CaseInsensitive));
    }
}
