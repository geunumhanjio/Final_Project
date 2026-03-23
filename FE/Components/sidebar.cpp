/**
 * @file sidebar.cpp
 * @brief Sidebar implementation
 */
#include "sidebar.h"
#include "channelcatalog.h"

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
    if (patrolModeButton && patrolModeButton->isChecked()) {
        return PatrolMode;
    }
    if (controlModeButton && controlModeButton->isChecked()) {
        return ControlMode;
    }
    if (autoModeButton && autoModeButton->isChecked()) {
        return AutoMode;
    }
    return ManualMode;
}

Sidebar::SettingsSection Sidebar::currentSettingsSection() const
{
    if (settingsButtonGroup) {
        const int checkedId = settingsButtonGroup->checkedId();
        if (checkedId >= 0) {
            return static_cast<SettingsSection>(checkedId);
        }
    }

    return CameraSettingsSection;
}

bool Sidebar::isControlButtonActive() const
{
    return activateControlButton && activateControlButton->isChecked();
}

bool Sidebar::isPatrolAddPointActive() const
{
    return addPatrolPointButton && addPatrolPointButton->isChecked();
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

void Sidebar::setPatrolAddPointActive(bool active)
{
    if (!addPatrolPointButton) {
        return;
    }

    const QString nextText = active ? QStringLiteral("Adding Points") : QStringLiteral("Add Point");
    const bool stateChanged = (addPatrolPointButton->isChecked() != active);
    const bool textChanged = (addPatrolPointButton->text() != nextText);

    if (!stateChanged && !textChanged) {
        return;
    }

    addPatrolPointButton->blockSignals(true);
    addPatrolPointButton->setChecked(active);
    addPatrolPointButton->setText(nextText);
    addPatrolPointButton->blockSignals(false);

    emit patrolAddPointToggled(active);
}

void Sidebar::setPatrolPointCount(int count)
{
    m_patrolPointCount = qMax(0, count);
    if (!finalizePatrolButton) {
        return;
    }

    finalizePatrolButton->setEnabled((currentRobotMode() == PatrolMode) && m_patrolPointCount > 0);
}

void Sidebar::setRobotMode(RobotMode mode)
{
    const bool modeChanged = (currentRobotMode() != mode);
    applyRobotModeSelection(mode, modeChanged);
}

void Sidebar::setSettingsSection(SettingsSection section)
{
    if (!settingsButtonGroup) {
        return;
    }

    if (QAbstractButton *button = settingsButtonGroup->button(section)) {
        const bool sectionChanged = (currentSettingsSection() != section);
        if (!button->isChecked()) {
            const QSignalBlocker blocker(settingsButtonGroup);
            button->setChecked(true);
        }

        if (sectionChanged) {
            emit settingsSectionSelected(section);
        }
    }
}

void Sidebar::setupUi()
{
    container = new QWidget(this);
    container->setObjectName("SidebarContainer");

    mainStack = new QStackedLayout(container);
    mainStack->setContentsMargins(0, 0, 0, 0);

    setupLiveUI();
    setupPlaybackUI();
    setupSettingsUI();

    mainStack->addWidget(liveWidget);
    mainStack->addWidget(playbackWidget);
    mainStack->addWidget(settingsWidget);

    setWidget(container);
}

void Sidebar::setMode(SidebarMode mode)
{
    if (mode == Live) {
        mainStack->setCurrentIndex(0);
        setWindowTitle("Channel List");
    } else if (mode == Playback) {
        mainStack->setCurrentIndex(1);
        setWindowTitle("Recordings");
    } else {
        mainStack->setCurrentIndex(2);
        setWindowTitle("Settings");
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
    patrolModeButton = new QRadioButton("Patrol Mode", modePanel);
    manualModeButton->setObjectName("SidebarModeOption");
    autoModeButton->setObjectName("SidebarModeOption");
    controlModeButton->setObjectName("SidebarModeOption");
    patrolModeButton->setObjectName("SidebarModeOption");

    modeButtonGroup = new QButtonGroup(modePanel);
    modeButtonGroup->setExclusive(true);
    modeButtonGroup->addButton(manualModeButton, ManualMode);
    modeButtonGroup->addButton(autoModeButton, AutoMode);
    modeButtonGroup->addButton(controlModeButton, ControlMode);
    modeButtonGroup->addButton(patrolModeButton, PatrolMode);
    manualModeButton->setChecked(true);

    modeLayout->addWidget(manualModeButton);
    modeLayout->addWidget(autoModeButton);
    modeLayout->addWidget(controlModeButton);
    modeLayout->addWidget(patrolModeButton);

    activateControlButton = new QPushButton("Start Control", modePanel);
    activateControlButton->setObjectName("SidebarControlButton");
    activateControlButton->setMinimumHeight(40);
    activateControlButton->setCheckable(true);
    activateControlButton->setEnabled(false);
    modeLayout->addWidget(activateControlButton);

    addPatrolPointButton = new QPushButton("Add Point", modePanel);
    addPatrolPointButton->setObjectName("SidebarControlButton");
    addPatrolPointButton->setMinimumHeight(40);
    addPatrolPointButton->setCheckable(true);
    addPatrolPointButton->setVisible(false);
    addPatrolPointButton->setEnabled(false);
    modeLayout->addWidget(addPatrolPointButton);

    finalizePatrolButton = new QPushButton(QStringLiteral("설정 완료"), modePanel);
    finalizePatrolButton->setObjectName("SidebarControlButton");
    finalizePatrolButton->setMinimumHeight(40);
    finalizePatrolButton->setVisible(false);
    finalizePatrolButton->setEnabled(false);
    modeLayout->addWidget(finalizePatrolButton);

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
    connect(addPatrolPointButton, &QPushButton::toggled, this, [this](bool checked) {
        addPatrolPointButton->setText(checked ? "Adding Points" : "Add Point");
        emit patrolAddPointToggled(checked);
    });
    connect(finalizePatrolButton, &QPushButton::clicked, this, &Sidebar::patrolFinalizeRequested);
    connect(emergencyStopButton, &QPushButton::clicked, this, &Sidebar::emergencyStopRequested);

    applyRobotModeSelection(currentRobotMode(), false);
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
    const bool patrolModeSelected = (mode == PatrolMode);
    const bool stopAvailable = controlModeSelected || patrolModeSelected;
    if (activateControlButton) {
        activateControlButton->setEnabled(controlModeSelected);
        activateControlButton->setVisible(controlModeSelected);
    }
    if (addPatrolPointButton) {
        addPatrolPointButton->setVisible(patrolModeSelected);
        addPatrolPointButton->setEnabled(patrolModeSelected);
        if (!patrolModeSelected) {
            setPatrolAddPointActive(false);
        }
    }
    if (finalizePatrolButton) {
        finalizePatrolButton->setVisible(patrolModeSelected);
        finalizePatrolButton->setEnabled(patrolModeSelected && m_patrolPointCount > 0);
    }
    if (emergencyStopButton) {
        emergencyStopButton->setEnabled(stopAvailable);
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

    for (const PlaybackCategoryDefinition &category : ChannelCatalog::playbackCategories()) {
        QListWidgetItem *item = new QListWidgetItem(category.name);
        item->setData(Qt::UserRole, category.id);
        categoryList->addItem(item);
    }

    layout->addWidget(categoryList);
    connect(categoryList, &QListWidget::itemClicked, this, &Sidebar::onCategoryClicked);
}

void Sidebar::setupSettingsUI()
{
    settingsWidget = new QWidget(container);
    QVBoxLayout *layout = new QVBoxLayout(settingsWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QWidget *panel = new QWidget(settingsWidget);
    panel->setObjectName("SettingsSidebarPanel");
    QVBoxLayout *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(16, 20, 16, 20);
    panelLayout->setSpacing(12);

    QLabel *titleLabel = new QLabel(QStringLiteral("SETTINGS"), panel);
    titleLabel->setObjectName("SettingsSidebarTitle");

    QLabel *subtitleLabel = new QLabel(QStringLiteral("Choose a configuration group for the center panel."), panel);
    subtitleLabel->setObjectName("SettingsSidebarSubtitle");
    subtitleLabel->setWordWrap(true);

    cameraSettingsButton = new QPushButton(QStringLiteral("Camera Settings"), panel);
    cameraSettingsButton->setObjectName("SettingsSidebarButton");
    cameraSettingsButton->setCheckable(true);
    cameraSettingsButton->setMinimumHeight(44);

    robotCarSettingsButton = new QPushButton(QStringLiteral("Robot Car Settings"), panel);
    robotCarSettingsButton->setObjectName("SettingsSidebarButton");
    robotCarSettingsButton->setCheckable(true);
    robotCarSettingsButton->setMinimumHeight(44);

    settingsButtonGroup = new QButtonGroup(panel);
    settingsButtonGroup->setExclusive(true);
    settingsButtonGroup->addButton(cameraSettingsButton, CameraSettingsSection);
    settingsButtonGroup->addButton(robotCarSettingsButton, RobotCarSettingsSection);
    cameraSettingsButton->setChecked(true);

    panelLayout->addWidget(titleLabel);
    panelLayout->addWidget(subtitleLabel);
    panelLayout->addSpacing(8);
    panelLayout->addWidget(cameraSettingsButton);
    panelLayout->addWidget(robotCarSettingsButton);
    panelLayout->addStretch();

    layout->addWidget(panel, 1);

    connect(settingsButtonGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int id) {
        emit settingsSectionSelected(id);
    });
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
    return ChannelCatalog::sidebarTitleForIndex(index);
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
