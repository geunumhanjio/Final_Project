#include "mainwindow.h"

#include "authmanager.h"
#include "configmanager.h"
#include "frucvideoprocessor.h"
#include "framelessconfirmdialog.h"
#include "fullscreenview.h"
#include "logindialog.h"
#include "streammanager.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QThread>
#include <QTimer>

void MainWindow::toggleTheme()
{
    m_isDark = !m_isDark;
    loadTheme(m_isDark ? QStringLiteral("style/theme_dark.qss")
                       : QStringLiteral("style/theme_light.qss"));
    ConfigManager::instance().setDarkTheme(m_isDark);
}

void MainWindow::onConfigChanged()
{
    updateTopBarUserInfo();

    const QString newIp = ConfigManager::instance().getRobotIp();
    if (newIp == m_currentRobotWsUrl) {
        return;
    }

    qDebug() << "[MainWindow] Config changed. Updating Robot IP to:" << newIp;
    m_currentRobotWsUrl = newIp;
    m_rosClient->disconnect();
    m_rosClient->connectToHost(newIp);
}

bool MainWindow::reopenLoginDialog(const QString &message)
{
    QMessageBox::warning(this,
                         QStringLiteral("Authentication Required"),
                         message.isEmpty()
                             ? QStringLiteral("Your session expired. Please sign in again.")
                             : message);

    LoginDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    updateTopBarUserInfo();
    StreamManager::instance().loadConfig();
    AuthManager::instance().fetchCurrentUserProfile();
    return true;
}

void MainWindow::startFrucProcessing(const QString &filePath)
{
    const QString normalizedPath = QDir::cleanPath(filePath);
    if (!FrucVideoProcessor::shouldProcessSourceFile(normalizedPath)) {
        return;
    }

    const auto startVariant = [this, normalizedPath](FrucVideoProcessor::Variant variant) {
        const QString outputPath = FrucVideoProcessor::outputPathFor(normalizedPath, variant);
        const QFileInfo outputInfo(outputPath);
        if (outputInfo.exists() && outputInfo.size() > 1024) {
            qDebug() << "[MainWindow] FRUC recording already exists:" << outputPath;
            if (m_playbackPage) {
                m_playbackPage->addLocalItem(outputPath);
            }
            return;
        }

        if (m_pendingFrucJobs.contains(outputPath)) {
            return;
        }

        m_pendingFrucJobs.insert(outputPath);
        auto *processor = new FrucVideoProcessor(normalizedPath, variant, this);

        connect(processor, &FrucVideoProcessor::processingFinished, this,
                [this, outputPath](const QString &, const QString &generatedPath) {
                    m_pendingFrucJobs.remove(outputPath);
                    qDebug() << "[MainWindow] FRUC recording generated:" << generatedPath;
                    if (m_playbackPage) {
                        m_playbackPage->addLocalItem(generatedPath);
                    }
                });

        connect(processor, &FrucVideoProcessor::processingFailed, this,
                [this, outputPath](const QString &, const QString &message) {
                    m_pendingFrucJobs.remove(outputPath);
                    qWarning() << "[MainWindow] FRUC processing failed for" << outputPath << ":" << message;
                });

        connect(processor, &QThread::finished, processor, &QObject::deleteLater);
        processor->start(QThread::LowPriority);
    };

    startVariant(FrucVideoProcessor::Variant::Fast);
    startVariant(FrucVideoProcessor::Variant::HighQuality);
}

void MainWindow::updateTopBarUserInfo()
{
    if (!m_topBar) {
        return;
    }

    m_topBar->setCurrentUserInfo(ConfigManager::instance().getActiveUserId(),
                                 ConfigManager::instance().getActiveUserEmail(),
                                 ConfigManager::instance().getLoginServerUrl());
}

void MainWindow::promptLogout()
{
    if (!showLogoutConfirmationDialog(this, m_isDark)) {
        return;
    }

    AuthManager::instance().clearSession();
    ConfigManager::instance().clearActiveLogin();
    updateTopBarUserInfo();

    if (m_fullPage) {
        m_fullPage->stop();
    }
    if (m_livePage) {
        m_livePage->stopAll();
    }

    const bool previousQuitOnLastWindowClosed = QApplication::quitOnLastWindowClosed();
    QApplication::setQuitOnLastWindowClosed(false);
    hide();

    QTimer::singleShot(0, this, [this, previousQuitOnLastWindowClosed]() {
        LoginDialog dialog(nullptr);
        if (dialog.exec() == QDialog::Accepted) {
            QApplication::setQuitOnLastWindowClosed(previousQuitOnLastWindowClosed);
            updateTopBarUserInfo();
            StreamManager::instance().loadConfig();
            AuthManager::instance().fetchCurrentUserProfile();
            show();
            raise();
            activateWindow();
            return;
        }

        QApplication::setQuitOnLastWindowClosed(previousQuitOnLastWindowClosed);
        m_skipCloseConfirmation = true;
        qApp->quit();
    });
}
