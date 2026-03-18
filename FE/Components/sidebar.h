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
    enum SidebarMode { Live, Playback };
    Q_ENUM(SidebarMode)

    enum RobotMode { ManualMode, AutoMode, ControlMode };
    Q_ENUM(RobotMode)

    explicit Sidebar(const QString &title, QWidget *parent = nullptr);
    RobotMode currentRobotMode() const;
    bool isControlButtonActive() const;
    void setControlButtonActive(bool active);
    void setRobotMode(RobotMode mode);

public slots:
    void setMode(SidebarMode mode);

signals:
    void channelStateChanged(int channelIndex, bool isVisible);
    void categorySelected(int categoryId);
    void robotModeChanged(Sidebar::RobotMode mode);
    void controlButtonToggled(bool enabled);
    void emergencyStopRequested();

private:
    QWidget *container;
    QStackedLayout *mainStack;
    QWidget *liveWidget;
    QWidget *playbackWidget;

    QLineEdit *searchBar;
    QListWidget *channelList;
    QButtonGroup *modeButtonGroup;
    QRadioButton *manualModeButton;
    QRadioButton *autoModeButton;
    QRadioButton *controlModeButton;
    QPushButton *activateControlButton;
    QPushButton *emergencyStopButton;

    QListWidget *categoryList;

    void setupUi();
    void setupLiveUI();
    void setupPlaybackUI();
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
