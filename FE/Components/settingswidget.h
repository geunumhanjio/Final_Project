#ifndef SETTINGSWIDGET_H
#define SETTINGSWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>
#include "configmanager.h"
#include "streammanager.h"

class SettingsWidget : public QWidget {
    Q_OBJECT
public:
    SettingsWidget(QWidget *parent = nullptr) : QWidget(parent) {
        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        
        // 폼 레이아웃 (라벨: 입력창)
        QWidget *formContainer = new QWidget;
        formContainer->setStyleSheet("background-color: #1e1e1e; border-radius: 8px; padding: 20px;");
        QFormLayout *form = new QFormLayout(formContainer);
        
        ipEdit = new QLineEdit(this);
        portEdit = new QLineEdit(this);
        robotIpEdit = new QLineEdit(this); // [New] Robot IP
        
        rtspsOptionCheck = new QCheckBox("Use Secure RTSPS (Port 8322)", this); // [New]
        rtspsOptionCheck->setStyleSheet(
            "QCheckBox { color: #ffffff; font-weight: bold; font-size: 16px; padding: 5px; background: transparent; }"
            "QCheckBox::indicator { width: 24px; height: 24px; border: 2px solid #F59E0B; background: #222; border-radius: 4px; }"
            "QCheckBox::indicator:checked { background-color: #F59E0B; border-color: #F59E0B; }"
        );
        
        cctvOptionCheck = new QCheckBox("Use Custom CCTV URL", this); // Initialize cctvOptionCheck
        cctvOptionCheck->setStyleSheet(
            "QCheckBox { color: #ffffff; font-weight: bold; font-size: 16px; padding: 5px; background: transparent; }"
            "QCheckBox::indicator { width: 24px; height: 24px; border: 2px solid #00b2a9; background: #222; border-radius: 4px; }"
            "QCheckBox::indicator:unchecked:hover { border-color: #00e0d5; }"
            // Add a simple checkmark using border-image or just color difference if icon missing
            // Since we don't have an icon, let's make the checked state VERY obvious with a bright color
             "QCheckBox::indicator:checked { background-color: #00ff00; border-color: #00ff00; }"
        );
        
        // 현재 설정값 불러와서 채우기
        // Ensure defaults are loaded first just in case
        ConfigManager::instance().loadDefaults(); 
        ipEdit->setText(ConfigManager::instance().getCameraIp());
        portEdit->setText(ConfigManager::instance().getCameraPort());
        robotIpEdit->setText(ConfigManager::instance().getRobotIp()); // [New]
        cctvOptionCheck->setChecked(ConfigManager::instance().getUseCustomCCTV());
        rtspsOptionCheck->setChecked(ConfigManager::instance().getUseRtsps()); // [New]

        manualControlCheck = new QCheckBox("Enable Manual Control (WASD)", this);
        manualControlCheck->setStyleSheet(cctvOptionCheck->styleSheet());
        manualControlCheck->setChecked(ConfigManager::instance().getManualControl());
        
        // 스타일링
        QString inputStyle = "QLineEdit { padding: 8px; border: 1px solid #444; border-radius: 4px; color: white; background: #333; }";
        ipEdit->setStyleSheet(inputStyle);
        portEdit->setStyleSheet(inputStyle);
        robotIpEdit->setStyleSheet(inputStyle);
        
        QLabel *ipLabel = new QLabel("Camera IP:", this);
        ipLabel->setStyleSheet("color: #ddd; font-weight: bold;");
        QLabel *portLabel = new QLabel("RTSP Port:", this);
        portLabel->setStyleSheet("color: #ddd; font-weight: bold;");
        
        QLabel *optionLabel = new QLabel("CCTV Option:", this);
        optionLabel->setStyleSheet("color: #ddd; font-weight: bold;");
        QLabel *optionHint = new QLabel("Check to use: rtsp://admin:5hanwha!@<IP>:<PORT>/<ID>/H.264/media.smp", this);
        optionHint->setStyleSheet("color: #888; font-size: 11px; margin-bottom: 8px;");

        QLabel *rtspsLabel = new QLabel("Security Mode:", this); // [New]
        rtspsLabel->setStyleSheet("color: #ddd; font-weight: bold;");

        QLabel *robotIpLabel = new QLabel("ROS2 Bridge WS:", this); // [New]
        robotIpLabel->setStyleSheet("color: #ddd; font-weight: bold;");

        form->addRow(ipLabel, ipEdit);
        form->addRow(portLabel, portEdit);
        form->addRow(rtspsLabel, rtspsOptionCheck); // [New]
        form->addRow(optionLabel, cctvOptionCheck);
        form->addRow("", optionHint);
        form->addRow(robotIpLabel, robotIpEdit); // [New]
        
        QLabel *controlLabel = new QLabel("Control:", this);
        controlLabel->setStyleSheet("color: #ddd; font-weight: bold;");
        form->addRow(controlLabel, manualControlCheck);
        
        // 저장 버튼
        QPushButton *saveBtn = new QPushButton("Save Settings", this);
        saveBtn->setCursor(Qt::PointingHandCursor);
        saveBtn->setStyleSheet(
            "QPushButton { background-color: #00b2a9; color: white; padding: 10px; border-radius: 4px; font-weight: bold; font-size: 14px; }"
            "QPushButton:hover { background-color: #00807a; }"
            "QPushButton:pressed { background-color: #005f5b; }"
        );
        
        connect(saveBtn, &QPushButton::clicked, this, &SettingsWidget::saveSettings);
        
        mainLayout->addStretch();
        QLabel *title = new QLabel("System Configuration", this);
        title->setStyleSheet("color: white; font-size: 24px; font-weight: bold; margin-bottom: 20px;");
        mainLayout->addWidget(title, 0, Qt::AlignCenter);
        
        mainLayout->addWidget(formContainer);
        mainLayout->addSpacing(20);
        mainLayout->addWidget(saveBtn);
        mainLayout->addStretch();
        
        // Center the content horizontally in a fixed width if window is wide
        // But for now, just let it stretch with margins
        mainLayout->setContentsMargins(50, 20, 50, 20);
    }

    private slots:
    void saveSettings() {
        // 1. 설정 저장
        ConfigManager::instance().setCameraIp(ipEdit->text());
        ConfigManager::instance().setCameraPort(portEdit->text());
        ConfigManager::instance().setRobotIp(robotIpEdit->text()); // [New]
        ConfigManager::instance().setUseCustomCCTV(cctvOptionCheck->isChecked());
        ConfigManager::instance().setUseRtsps(rtspsOptionCheck->isChecked()); // [New]
        ConfigManager::instance().setManualControl(manualControlCheck->isChecked());
        
        // 2. 스트림 매니저 리로드 (URL 재생성)
        StreamManager::instance().loadConfig();
        
        QMessageBox::information(this, "Saved", "Settings saved successfully!\nManual Control settings applied instantly.");
    }

private:
    QLineEdit *ipEdit;
    QLineEdit *portEdit;
    QLineEdit *robotIpEdit; // [New]
    QCheckBox *rtspsOptionCheck; // [New]
    QCheckBox *cctvOptionCheck;
    QCheckBox *manualControlCheck;
};

#endif // SETTINGSWIDGET_H
