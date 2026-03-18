#ifndef SETTINGSWIDGET_H
#define SETTINGSWIDGET_H

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QWidget>
#include "configmanager.h"
#include "streammanager.h"

class SettingsWidget : public QWidget {
    Q_OBJECT
public:
    SettingsWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setObjectName("SettingsPage");

        auto markLabel = [](QLabel *label, const char *role) {
            label->setProperty("settingsRole", role);
            const bool isHint = QByteArray(role) == "hint";
            label->setWordWrap(isHint);
            if (isHint) {
                label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
                label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
                label->setMinimumHeight((label->fontMetrics().lineSpacing() * 2) + 8);
            }
        };
        auto markInput = [](QWidget *widget) {
            widget->setProperty("settingsRole", "input");
        };
        auto markCheck = [](QCheckBox *check) {
            check->setProperty("settingsRole", "check");
        };

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setSpacing(18);
        mainLayout->setContentsMargins(48, 28, 48, 28);

        QWidget *formContainer = new QWidget(this);
        formContainer->setObjectName("SettingsFormContainer");
        formContainer->setMaximumWidth(820);

        QFormLayout *form = new QFormLayout(formContainer);
        form->setContentsMargins(24, 24, 24, 24);
        form->setHorizontalSpacing(18);
        form->setVerticalSpacing(14);
        form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        ipEdit = new QLineEdit(formContainer);
        portEdit = new QLineEdit(formContainer);
        robotIpEdit = new QLineEdit(formContainer);
        manualLinearSpin = new QDoubleSpinBox(formContainer);
        manualAngularSpin = new QDoubleSpinBox(formContainer);
        markInput(ipEdit);
        markInput(portEdit);
        markInput(robotIpEdit);
        markInput(manualLinearSpin);
        markInput(manualAngularSpin);

        rtspsOptionCheck = new QCheckBox("Use Secure RTSPS (Port 8322)", formContainer);
        cctvOptionCheck = new QCheckBox("Use Custom CCTV URL", formContainer);
        manualControlCheck = new QCheckBox("Enable Manual Control (WASD)", formContainer);
        markCheck(rtspsOptionCheck);
        markCheck(cctvOptionCheck);
        markCheck(manualControlCheck);

        ConfigManager::instance().loadDefaults();
        ipEdit->setText(ConfigManager::instance().getCameraIp());
        portEdit->setText(ConfigManager::instance().getCameraPort());
        robotIpEdit->setText(ConfigManager::instance().getRobotHost());
        robotIpEdit->setPlaceholderText("172.20.26.8");
        cctvOptionCheck->setChecked(ConfigManager::instance().getUseCustomCCTV());
        rtspsOptionCheck->setChecked(ConfigManager::instance().getUseRtsps());
        manualControlCheck->setChecked(ConfigManager::instance().getManualControl());

        manualLinearSpin->setDecimals(2);
        manualLinearSpin->setRange(0.0, 1.0);
        manualLinearSpin->setSingleStep(0.01);
        manualLinearSpin->setValue(ConfigManager::instance().getManualLinearX());

        manualAngularSpin->setDecimals(2);
        manualAngularSpin->setRange(0.0, 1.0);
        manualAngularSpin->setSingleStep(0.01);
        manualAngularSpin->setValue(ConfigManager::instance().getManualAngularZ());

        QLabel *title = new QLabel("System Configuration", this);
        title->setObjectName("SettingsTitle");
        markLabel(title, "title");

        QLabel *cameraLabel = new QLabel("Camera IP:", formContainer);
        QLabel *portLabel = new QLabel("RTSP Port:", formContainer);
        QLabel *securityLabel = new QLabel("Security Mode:", formContainer);
        QLabel *cctvLabel = new QLabel("CCTV Option:", formContainer);
        QLabel *cctvHint = new QLabel("Check to use: rtsp://admin:5hanwha!@<IP>:<PORT>/<ID>/H.264/media.smp", formContainer);
        QLabel *robotLabel = new QLabel("ROS2 Bridge Host:", formContainer);
        QLabel *robotHint = new QLabel("Enter only the host/IP. WS becomes ws://<host>:9090 and RC RTSP becomes rtsp://<host>:9554/camera", formContainer);
        QLabel *controlLabel = new QLabel("Control:", formContainer);
        QLabel *linearLabel = new QLabel("Linear X:", formContainer);
        QLabel *angularLabel = new QLabel("Angular Z:", formContainer);
        QLabel *controlHint = new QLabel("Set manual WASD speeds from 0.00 to 1.00.", formContainer);

        markLabel(cameraLabel, "label");
        markLabel(portLabel, "label");
        markLabel(securityLabel, "label");
        markLabel(cctvLabel, "label");
        markLabel(cctvHint, "hint");
        markLabel(robotLabel, "label");
        markLabel(robotHint, "hint");
        markLabel(controlLabel, "section");
        markLabel(linearLabel, "label");
        markLabel(angularLabel, "label");
        markLabel(controlHint, "hint");

        form->addRow(cameraLabel, ipEdit);
        form->addRow(portLabel, portEdit);
        form->addRow(securityLabel, rtspsOptionCheck);
        form->addRow(cctvLabel, cctvOptionCheck);
        form->addRow("", cctvHint);
        form->addRow(robotLabel, robotIpEdit);
        form->addRow("", robotHint);
        form->addRow(controlLabel, manualControlCheck);
        form->addRow(linearLabel, manualLinearSpin);
        form->addRow(angularLabel, manualAngularSpin);
        form->addRow("", controlHint);

        QPushButton *saveBtn = new QPushButton("Save Settings", this);
        saveBtn->setObjectName("SettingsSaveButton");
        saveBtn->setCursor(Qt::PointingHandCursor);
        saveBtn->setFixedWidth(220);
        saveBtn->setMinimumHeight(44);
        connect(saveBtn, &QPushButton::clicked, this, &SettingsWidget::saveSettings);

        mainLayout->addStretch();
        mainLayout->addWidget(title, 0, Qt::AlignCenter);
        mainLayout->addWidget(formContainer, 0, Qt::AlignHCenter);
        mainLayout->addWidget(saveBtn, 0, Qt::AlignHCenter);
        mainLayout->addStretch();
    }

private slots:
    void saveSettings() {
        ConfigManager::instance().setCameraIp(ipEdit->text());
        ConfigManager::instance().setCameraPort(portEdit->text());
        ConfigManager::instance().setRobotIp(robotIpEdit->text().trimmed());
        ConfigManager::instance().setUseCustomCCTV(cctvOptionCheck->isChecked());
        ConfigManager::instance().setUseRtsps(rtspsOptionCheck->isChecked());
        ConfigManager::instance().setManualControl(manualControlCheck->isChecked());
        ConfigManager::instance().setManualLinearX(manualLinearSpin->value());
        ConfigManager::instance().setManualAngularZ(manualAngularSpin->value());

        StreamManager::instance().loadConfig();

        QMessageBox::information(this, "Saved", "Settings saved successfully!\nManual Control settings applied instantly.");
    }

private:
    QLineEdit *ipEdit;
    QLineEdit *portEdit;
    QLineEdit *robotIpEdit;
    QCheckBox *rtspsOptionCheck;
    QCheckBox *cctvOptionCheck;
    QCheckBox *manualControlCheck;
    QDoubleSpinBox *manualLinearSpin;
    QDoubleSpinBox *manualAngularSpin;
};

#endif // SETTINGSWIDGET_H
