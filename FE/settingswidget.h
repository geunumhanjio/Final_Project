#ifndef SETTINGSWIDGET_H
#define SETTINGSWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
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
        
        // 현재 설정값 불러와서 채우기
        // Ensure defaults are loaded first just in case
        ConfigManager::instance().loadDefaults(); 
        ipEdit->setText(ConfigManager::instance().getCameraIp());
        portEdit->setText(ConfigManager::instance().getCameraPort());
        
        // 스타일링
        QString inputStyle = "QLineEdit { padding: 8px; border: 1px solid #444; border-radius: 4px; color: white; background: #333; }";
        ipEdit->setStyleSheet(inputStyle);
        portEdit->setStyleSheet(inputStyle);
        
        QLabel *ipLabel = new QLabel("Camera IP:", this);
        ipLabel->setStyleSheet("color: #ddd; font-weight: bold;");
        QLabel *portLabel = new QLabel("RTSP Port:", this);
        portLabel->setStyleSheet("color: #ddd; font-weight: bold;");

        form->addRow(ipLabel, ipEdit);
        form->addRow(portLabel, portEdit);
        
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
        
        // 2. 스트림 매니저 리로드 (URL 재생성)
        StreamManager::instance().loadConfig();
        
        QMessageBox::information(this, "Saved", "Settings saved successfully!\nPlease restart streams to apply.");
    }

private:
    QLineEdit *ipEdit;
    QLineEdit *portEdit;
};

#endif // SETTINGSWIDGET_H
