#include "mainwindow.h"

#include "authmanager.h"
#include "channelcatalog.h"
#include "configmanager.h"
#include "framelessconfirmdialog.h"
#include "fullscreenview.h"
#include "playbackview.h"
#include "settingswidget.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>
#include <QUrl>

namespace {

QString currentCameraIp()
{
    return ConfigManager::instance().getCameraIp();
}

QString fileNameFromRecordingUrl(const QString &url)
{
    QString filename;
    const QUrl recordUrl(url);
    if (recordUrl.isValid() && !recordUrl.path().isEmpty()) {
        filename = QFileInfo(recordUrl.path()).fileName();
    }
    if (filename.isEmpty()) {
        filename = QFileInfo(url).fileName();
    }
    return filename;
}

QString downloadPathFor(const QString &fileName)
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).filePath(fileName);
}

QString calibrationControlServerIp()
{
    return currentCameraIp();
}

} // namespace

void MainWindow::loadTheme(const QString &relativePath)
{
    const QString resourcePath = QStringLiteral(":/") + relativePath;
    QString fullPath = resourcePath;

    if (!QFile::exists(fullPath)) {
        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList candidates = {
            QDir(appDir).filePath(relativePath),
            QDir(appDir).filePath(QStringLiteral("../") + relativePath),
            QDir(appDir).filePath(QStringLiteral("../../") + relativePath),
            QDir(appDir).filePath(QStringLiteral("../../../") + relativePath),
            QDir::current().filePath(relativePath)
        };

        for (const QString &candidate : candidates) {
            if (QFile::exists(candidate)) {
                fullPath = candidate;
                break;
            }
        }
    }

    if (!QFile::exists(fullPath)) {
        qWarning() << "Theme file does not exist:" << relativePath;
        return;
    }

    QFile file(fullPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qApp->setStyleSheet(QLatin1String(file.readAll()));
        qDebug() << "Theme loaded from:" << fullPath;
        file.close();
    } else {
        qDebug() << "FAILED to load theme from:" << fullPath;
    }
}

void MainWindow::initUI()
{
    m_topBar = new TopBar(this);
    setMenuWidget(m_topBar);

    m_sidebar = new Sidebar(QStringLiteral("채널 목록"), this);
    addDockWidget(Qt::LeftDockWidgetArea, m_sidebar);

    qDebug() << "[MainWindow] Creating Central Stack...";
    m_centralStack = new QStackedWidget(this);
    m_centralStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    setCentralWidget(m_centralStack);

    qDebug() << "[MainWindow] Creating LiveView...";
    m_livePage = new LiveView(this);

    qDebug() << "[MainWindow] Creating PlaybackView...";
    m_playbackPage = new PlaybackView(this);

    qDebug() << "[MainWindow] Creating SettingsWidget...";
    m_settingsPage = new SettingsWidget(this);

    qDebug() << "[MainWindow] Creating FullScreenView...";
    m_fullPage = new FullScreenView(this);

    qDebug() << "[MainWindow] Adding widgets to stack...";
    m_livePage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    m_playbackPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    m_settingsPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    m_fullPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);

    m_centralStack->addWidget(m_livePage);
    m_centralStack->addWidget(m_playbackPage);
    m_centralStack->addWidget(m_settingsPage);
    m_centralStack->addWidget(m_fullPage);

    qDebug() << "[MainWindow] initUI Completed.";
}

void MainWindow::initConnections()
{
    qDebug() << "[MainWindow] initConnections Started...";

    connect(m_topBar, &TopBar::sidebarToggled, [this]() {
        if (!m_sidebar || !m_centralStack || m_centralStack->currentWidget() == m_fullPage) {
            return;
        }
        m_sidebar->setVisible(!m_sidebar->isVisible());
    });

    connect(m_topBar, &TopBar::modeChanged, [this](int index) {
        m_centralStack->setCurrentIndex(index);
        if (index == 1) {
            m_sidebar->setMode(Sidebar::Playback);
        } else if (index == 2) {
            m_sidebar->setMode(Sidebar::Settings);
            if (m_settingsPage) {
                m_settingsPage->setSection(static_cast<SettingsWidget::Section>(m_sidebar->currentSettingsSection()));
            }
        } else {
            m_sidebar->setMode(Sidebar::Live);
        }
    });

    connect(m_topBar, &TopBar::themeToggled, this, &MainWindow::toggleTheme);
    connect(m_topBar, &TopBar::closeRequested, this, &QWidget::close);
    connect(m_topBar, &TopBar::logoutRequested, this, &MainWindow::promptLogout);

    connect(m_centralStack, &QStackedWidget::currentChanged, this, [this](int) {
        updateSidebarForCurrentPage();
        applySharedVideoGoalOverlay();
    });

    connect(m_settingsPage, &SettingsWidget::autoNavSpeedApplyRequested, this, [this](double speed) {
        if (m_rosClient) {
            m_rosClient->sendNavSetSpeed(speed);
        }
    });

    connect(m_sidebar, &Sidebar::settingsSectionSelected, this, [this](int sectionId) {
        if (!m_settingsPage) {
            return;
        }
        m_settingsPage->setSection(static_cast<SettingsWidget::Section>(sectionId));
    });

    connect(m_sidebar, &Sidebar::channelStateChanged, m_livePage, &LiveView::setChannelVisible);
    connect(m_sidebar, &Sidebar::robotModeChanged, this, &MainWindow::applyRobotMode);

    connect(m_sidebar, &Sidebar::patrolAddPointToggled, this, [this](bool enabled) {
        if (m_robotMode != Sidebar::PatrolMode || !m_livePage) {
            if (enabled && m_sidebar) {
                m_sidebar->setPatrolAddPointActive(false);
            }
            return;
        }

        m_livePage->setPatrolAddPointMode(enabled);
    });

    connect(m_sidebar, &Sidebar::patrolFinalizeRequested, this, &MainWindow::finalizePatrolPath);
    connect(m_sidebar, &Sidebar::emergencyStopRequested, this, &MainWindow::requestEmergencyStop);

    connect(m_sidebar, &Sidebar::controlButtonToggled, this, [this](bool enabled) {
        const bool active = enabled && (m_robotMode == Sidebar::ControlMode);
        if (active && m_rosClient) {
            m_rosClient->sendModeControl(QStringLiteral("auto"));
        }
        m_livePage->setGoalTargetingEnabled(active);
        m_fullPage->setControlModeAvailable(m_robotMode == Sidebar::ControlMode);
        m_fullPage->setControlModeChecked(active);
    });

    connect(m_fullPage, &FullScreenView::controlModeRequested, this, [this](bool enabled) {
        if (!m_sidebar) {
            return;
        }

        if (enabled) {
            if (m_sidebar->currentRobotMode() != Sidebar::ControlMode) {
                m_sidebar->setRobotMode(Sidebar::ControlMode);
            } else {
                m_sidebar->setControlButtonActive(true);
            }
        } else {
            deactivateControlSession();
        }
    });

    connect(m_fullPage, &FullScreenView::robotModeSelectionRequested, this, [this](int mode) {
        if (!m_sidebar) {
            return;
        }

        m_sidebar->setRobotMode(static_cast<Sidebar::RobotMode>(mode));
    });

    connect(m_fullPage, &FullScreenView::emergencyStopRequested, this, &MainWindow::requestEmergencyStop);
    connect(m_livePage, &LiveView::goalInteractionStarted, this, &MainWindow::clearAllGoalOverlays);
    connect(m_livePage, &LiveView::videoGoalOverlayCommitted, this, &MainWindow::setSharedVideoGoalOverlay);
    connect(m_livePage, &LiveView::goalCommitted, this, &MainWindow::deactivateControlSession);
    connect(m_livePage, &LiveView::patrolPointsChanged, m_sidebar, &Sidebar::setPatrolPointCount);
    connect(m_fullPage, &FullScreenView::goalInteractionStarted, this, &MainWindow::clearAllGoalOverlays);
    connect(m_fullPage, &FullScreenView::videoGoalOverlayCommitted, this, &MainWindow::setSharedVideoGoalOverlay);
    connect(m_fullPage, &FullScreenView::videoGoalOverlayClearRequested, this, &MainWindow::clearSharedVideoGoalOverlay);
    connect(m_fullPage, &FullScreenView::goalCommitted, this, &MainWindow::deactivateControlSession);
    connect(m_livePage, &LiveView::calibrationClickRequested, this,
            [this](int channelIndex, double x1, double y1, double x2, double y2) {
                if (!m_cameraClient || channelIndex != 1) {
                    return;
                }

                const QString serverIp = calibrationControlServerIp();
                m_cameraClient->sendCalibrationClick(serverIp, x1, y1, x2, y2);
                qDebug().noquote() << QStringLiteral("[MainWindow] Sent CALIBRATION_CLICK to ws://%1:9000 x1=%2 y1=%3 x2=%4 y2=%5")
                                          .arg(serverIp)
                                          .arg(x1, 0, 'f', 4)
                                          .arg(y1, 0, 'f', 4)
                                          .arg(x2, 0, 'f', 4)
                                          .arg(y2, 0, 'f', 4);
            });
    connect(m_fullPage, &FullScreenView::calibrationClickRequested, this,
            [this](int channelIndex, double x1, double y1, double x2, double y2) {
                if (!m_cameraClient || channelIndex != 1) {
                    return;
                }

                const QString serverIp = calibrationControlServerIp();
                m_cameraClient->sendCalibrationClick(serverIp, x1, y1, x2, y2);
                qDebug().noquote() << QStringLiteral("[MainWindow] Sent CALIBRATION_CLICK to ws://%1:9000 x1=%2 y1=%3 x2=%4 y2=%5")
                                          .arg(serverIp)
                                          .arg(x1, 0, 'f', 4)
                                          .arg(y1, 0, 'f', 4)
                                          .arg(x2, 0, 'f', 4)
                                          .arg(y2, 0, 'f', 4);
            });

    connect(m_rosClient, &RosBridgeClient::mapReceived, m_livePage, &LiveView::updateMap);
    connect(m_rosClient, &RosBridgeClient::mapReceived, this, [this](const QJsonObject &data) {
        const QJsonObject info = data.value(QStringLiteral("info")).toObject();
        const QJsonObject origin = info.value(QStringLiteral("origin")).toObject();
        m_fullPage->setMapGeometry(QPointF(origin.value(QStringLiteral("x")).toDouble(),
                                           origin.value(QStringLiteral("y")).toDouble()),
                                   info.value(QStringLiteral("width")).toInt(),
                                   info.value(QStringLiteral("height")).toInt(),
                                   info.value(QStringLiteral("resolution")).toDouble());
    });
    connect(m_rosClient, &RosBridgeClient::odomReceived, m_livePage, &LiveView::updateOdom);
    connect(m_rosClient, &RosBridgeClient::odomReceived, this, &MainWindow::handleGoalOdomUpdate);
    connect(m_rosClient, &RosBridgeClient::pathReceived, m_livePage, &LiveView::updatePath);
    connect(m_rosClient, &RosBridgeClient::navStatusReceived, this, &MainWindow::handleGoalNavStatus);
    connect(m_rosClient, &RosBridgeClient::navFeedbackReceived, this, &MainWindow::handleGoalNavFeedback);

    connect(m_sidebar, &Sidebar::categorySelected, m_playbackPage, &PlaybackView::filterRecordings);

    connect(m_livePage, &LiveView::recordCommandRequested, this, [this](int channelId, bool start) {
        const QString ip = currentCameraIp();
        m_cameraClient->sendRecordCommand(ip, channelId, start);

        if (start) {
            qDebug() << "[Recording] Started on Channel" << channelId;
        } else {
            qDebug() << "[Recording] Stopped on Channel" << channelId;
        }
    });

    if (m_cameraClient) {
        connect(m_cameraClient, &CameraControlClient::slamMappingErrorReceived, this,
                [this](const QString &reason, double normalizedX, double normalizedY) {
                    const QString mappedReason = reason.trimmed().isEmpty()
                        ? QStringLiteral("unknown")
                        : reason.trimmed();
                    QMessageBox::warning(
                        this,
                        QStringLiteral("SLAM Mapping Error"),
                        QStringLiteral("Calibration click mapping failed.\nReason: %1\nNormalized: (%2, %3)")
                            .arg(mappedReason)
                            .arg(normalizedX, 0, 'f', 2)
                            .arg(normalizedY, 0, 'f', 2));
                });

        connect(m_playbackPage, &PlaybackView::refreshRequested, this, [this]() {
            m_cameraClient->requestRecordings(currentCameraIp());
        });

        connect(m_cameraClient, &CameraControlClient::recordingListReceived,
                m_playbackPage, &PlaybackView::updateList);

        connect(m_cameraClient, &CameraControlClient::videoReceived, this, [this](const QString &url) {
            qDebug() << "[MainWindow] Recording finished at:" << url << "- Refreshing list.";
            if (m_playbackPage) {
                emit m_playbackPage->refreshRequested();
            }

            const QString filename = fileNameFromRecordingUrl(url);
            if (filename.isEmpty()) {
                qWarning() << "[MainWindow] Could not infer recording filename from:" << url;
                return;
            }

            const QString ip = currentCameraIp();
            const QString localFilePath = downloadPathFor(filename);
            const QFileInfo localInfo(localFilePath);
            if (localInfo.exists() && localInfo.size() > 1024) {
                qDebug() << "[MainWindow] Original recording already exists locally:" << localFilePath;
                if (m_playbackPage) {
                    m_playbackPage->addLocalItem(localFilePath);
                }
                startFrucProcessing(localFilePath);
                return;
            }

            qDebug() << "[MainWindow] Auto-downloading completed LOW recording:" << filename;
            m_cameraClient->requestDownload(ip, filename);
        });

        connect(m_playbackPage, &PlaybackView::playRequested, this, [this](const QString &filename) {
            qDebug() << "[MainWindow] playRequested signal received for:" << filename;
            const QString localFilePath = downloadPathFor(filename);
            const QFileInfo fileInfo(localFilePath);

            if (fileInfo.exists() && fileInfo.size() > 1024) {
                qDebug() << "[MainWindow] Found valid local file:" << localFilePath << "Size:" << fileInfo.size();
                m_returnToWidget = m_playbackPage;
                m_fullPage->play(localFilePath, 0);
                m_centralStack->setCurrentWidget(m_fullPage);
                applySharedVideoGoalOverlay();
                return;
            }

            if (fileInfo.exists()) {
                qDebug() << "[MainWindow] Found invalid/small file (" << fileInfo.size() << "bytes). Deleting to re-download.";
                QFile::remove(localFilePath);
            }

            qDebug() << "[MainWindow] File not found local, requesting download:" << filename;
            m_cameraClient->requestDownload(currentCameraIp(), filename);
        });

        connect(m_cameraClient, &CameraControlClient::downloadProgress,
                m_playbackPage, &PlaybackView::updateDownloadProgress);

        connect(m_cameraClient, &CameraControlClient::downloadFinished, this, [this](const QString &filePath) {
            qDebug() << "[MainWindow] Download Finished:" << filePath;
            m_playbackPage->addLocalItem(filePath);
            startFrucProcessing(filePath);
        });
    } else {
        qCritical() << "[MainWindow] m_cameraClient is NULL in initConnections!";
    }

    connect(m_livePage, &LiveView::requestFullScreen, this, [this](int index, const QString &url) {
        if (index <= 4 && !url.isEmpty()) {
            qDebug() << "Full Screen Request:" << url;

            m_fullPage->setControlModeAvailable(m_robotMode == Sidebar::ControlMode);
            m_fullPage->setRobotModeSelection(static_cast<int>(m_robotMode));
            m_returnToWidget = m_livePage;
            m_centralStack->setCurrentWidget(m_fullPage);
            m_fullPage->play(url, index);
            m_fullPage->setControlModeChecked((m_robotMode == Sidebar::ControlMode) && m_sidebar->isControlButtonActive());
            applySharedVideoGoalOverlay();
        }
    });

    connect(m_fullPage, &FullScreenView::closeRequested, this, [this]() {
        if (m_isClosingFullScreen) {
            return;
        }

        m_isClosingFullScreen = true;
        QTimer::singleShot(0, this, [this]() {
            if (m_centralStack) {
                if (m_returnToWidget) {
                    m_centralStack->setCurrentWidget(m_returnToWidget);
                } else if (m_livePage) {
                    m_centralStack->setCurrentWidget(m_livePage);
                }
            }

            updateSidebarForCurrentPage();
            m_isClosingFullScreen = false;
        });
    });

    connect(m_fullPage, &FullScreenView::recordRequested, this, [this](int index, bool start) {
        const int actualChannelId = ChannelCatalog::fullScreenRecordChannelIdForIndex(index);
        if (actualChannelId < 0 || !m_cameraClient) {
            return;
        }

        m_cameraClient->sendRecordCommand(currentCameraIp(), actualChannelId, start);
        if (start) {
            qDebug() << "[FullScreen] Recording Started on Channel" << actualChannelId;
        } else {
            qDebug() << "[FullScreen] Recording Stopped on Channel" << actualChannelId;
        }
    });

    connect(m_fullPage, &FullScreenView::reqGoalPose, this, [this](double x, double y, double theta) {
        if (m_rosClient && m_robotMode == Sidebar::ControlMode) {
            armGoalTracking(QPointF(x, y), theta);
            m_rosClient->sendModeControl(QStringLiteral("auto"));
            m_rosClient->sendGoalPose(x, y, theta);
            deactivateControlSession();
        }
    });

    connect(m_livePage, &LiveView::goalPoseRequested, this, [this](double x, double y, double theta) {
        if (m_rosClient && m_robotMode == Sidebar::ControlMode) {
            armGoalTracking(QPointF(x, y), theta);
            m_rosClient->sendModeControl(QStringLiteral("auto"));
            m_rosClient->sendGoalPose(x, y, theta);
            deactivateControlSession();
        }
    });

    qDebug() << "[MainWindow] initConnections Completed.";
    applyRobotMode(m_sidebar->currentRobotMode());
    updateSidebarForCurrentPage();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_skipCloseConfirmation) {
        event->accept();
        return;
    }

    if (!showCloseConfirmationDialog(this, m_isDark)) {
        event->ignore();
        return;
    }

    if (m_livePage) {
        m_livePage->stopAll();
    }
    if (m_fullPage) {
        m_fullPage->stop();
    }

    event->accept();
}
