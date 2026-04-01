/**
 * @file sidebar.h
 * @brief Left sidebar header
 */
#ifndef SIDEBAR_H
#define SIDEBAR_H

#include <QDockWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

class QButtonGroup;
class QPushButton;
class QRadioButton;
class QStackedLayout;

class Sidebar : public QDockWidget
{
    Q_OBJECT

public:
    enum SidebarMode { Live, Playback, Settings };
    Q_ENUM(SidebarMode)

    enum RobotMode { ManualMode, AutoMode, ControlMode, PatrolMode };
    Q_ENUM(RobotMode)

    enum SettingsSection { CameraSettingsSection, RobotCarSettingsSection };
    Q_ENUM(SettingsSection)

    explicit Sidebar(const QString &title, QWidget *parent = nullptr);
    RobotMode currentRobotMode() const;
    SettingsSection currentSettingsSection() const;
    bool isControlButtonActive() const;
    bool isPatrolAddPointActive() const;
    void setControlButtonActive(bool active);
    void setPatrolAddPointActive(bool active);
    void setPatrolPointCount(int count);
    void setRobotMode(RobotMode mode);
    void setSettingsSection(SettingsSection section);

public slots:
    void setMode(SidebarMode mode);

signals:
    void channelStateChanged(int channelIndex, bool isVisible);
    void categorySelected(int categoryId);
    void robotModeChanged(Sidebar::RobotMode mode);
    void settingsSectionSelected(int sectionId);
    void controlButtonToggled(bool enabled);
    void patrolAddPointToggled(bool enabled);
    void patrolFinalizeRequested();
    void emergencyStopRequested();

private:
    QWidget *container;
    QStackedLayout *mainStack;
    QWidget *liveWidget;
    QWidget *playbackWidget;
    QWidget *settingsWidget;

    QLineEdit *searchBar;
    QListWidget *channelList;
    QButtonGroup *modeButtonGroup;
    QRadioButton *manualModeButton;
    QRadioButton *autoModeButton;
    QRadioButton *controlModeButton;
    QRadioButton *patrolModeButton;
    QPushButton *activateControlButton;
    QPushButton *addPatrolPointButton;
    QPushButton *finalizePatrolButton;
    QPushButton *emergencyStopButton;
    int m_patrolPointCount = 0;

    QListWidget *categoryList;
    QButtonGroup *settingsButtonGroup = nullptr;
    QPushButton *cameraSettingsButton = nullptr;
    QPushButton *robotCarSettingsButton = nullptr;

    void setupUi();
    void setupLiveUI();
    void setupPlaybackUI();
    void setupSettingsUI();
    void applyRobotModeSelection(RobotMode mode, bool emitModeSignal);

    void setupList();
    void addHeaderItem(QString title, QString count);
    void addChannelItem(int index, QString name, bool isLidar = false, bool useTextStatus = true);
    QString getChannelName(int index);
    void filterChannels(const QString &text);

private slots:
    void onItemClicked(QListWidgetItem *item);
    void onCategoryClicked(QListWidgetItem *item);
};

#endif // SIDEBAR_H
