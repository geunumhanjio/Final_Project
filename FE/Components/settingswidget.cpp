#include "settingswidget.h"

#include <QFormLayout>
#include <QFrame>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "configmanager.h"
#include "streammanager.h"

SettingsWidget::SettingsWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("SettingsPage");

    ConfigManager::instance().loadDefaults();

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setObjectName("SettingsScrollArea");
    rootLayout->addWidget(scrollArea);

    auto *content = new QWidget(scrollArea);
    scrollArea->setWidget(content);

    auto *mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(48, 28, 48, 28);
    mainLayout->setSpacing(18);

    m_titleLabel = new QLabel(content);
    m_titleLabel->setObjectName("SettingsTitle");

    m_sectionStack = new QStackedWidget(content);
    m_sectionStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_sectionStack->addWidget(createCenteredPage(createCameraSection()));
    m_sectionStack->addWidget(createCenteredPage(createRobotSection()));

    auto *saveButton = new QPushButton(QStringLiteral("Save Settings"), content);
    saveButton->setObjectName("SettingsSaveButton");
    saveButton->setCursor(Qt::PointingHandCursor);
    saveButton->setFixedWidth(220);
    saveButton->setMinimumHeight(44);
    connect(saveButton, &QPushButton::clicked, this, &SettingsWidget::saveSettings);

    mainLayout->addWidget(m_titleLabel, 0, Qt::AlignCenter);
    mainLayout->addWidget(m_sectionStack);
    mainLayout->addWidget(saveButton, 0, Qt::AlignHCenter);
    mainLayout->addStretch();

    setSection(CameraSection);
}

SettingsWidget::Section SettingsWidget::currentSection() const
{
    return m_currentSection;
}

void SettingsWidget::setSection(SettingsWidget::Section section)
{
    m_currentSection = section;
    if (m_sectionStack) {
        m_sectionStack->setCurrentIndex(static_cast<int>(section));
    }
    updateSectionTitle();
}

void SettingsWidget::saveSettings()
{
    ConfigManager::instance().setCameraIp(ipEdit->text());
    ConfigManager::instance().setCameraPort(portEdit->text());
    ConfigManager::instance().setCustomCctvUsername(customCctvUserEdit->text());
    ConfigManager::instance().setCustomCctvPassword(customCctvPasswordEdit->text());
    ConfigManager::instance().setRobotIp(robotIpEdit->text().trimmed());
    ConfigManager::instance().setUseCustomCCTV(cctvOptionCheck->isChecked());
    ConfigManager::instance().setUseRtsps(rtspsOptionCheck->isChecked());
    ConfigManager::instance().setManualControl(manualControlCheck->isChecked());
    ConfigManager::instance().setManualLinearX(manualLinearSpin->value());
    ConfigManager::instance().setManualAngularZ(manualAngularSpin->value());
    ConfigManager::instance().setAutoNavSpeed(autoNavSpeedSpin->value());

    StreamManager::instance().loadConfig();
    emit autoNavSpeedApplyRequested(autoNavSpeedSpin->value());

    QMessageBox::information(this,
                             QStringLiteral("Saved"),
                             QStringLiteral("Settings saved successfully.\nManual and auto navigation speed settings applied instantly."));
}

void SettingsWidget::markLabel(QLabel *label, const char *role)
{
    label->setProperty("settingsRole", role);
    const bool isHint = QByteArray(role) == "hint";
    label->setWordWrap(isHint);
    if (isHint) {
        label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
}

void SettingsWidget::markInput(QWidget *widget)
{
    widget->setProperty("settingsRole", "input");
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    widget->setMinimumHeight(44);
}

void SettingsWidget::markCheck(QCheckBox *check)
{
    check->setProperty("settingsRole", "check");
    check->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    check->setMinimumHeight(30);
}

void SettingsWidget::configureFormLayout(QFormLayout *form)
{
    form->setContentsMargins(24, 24, 24, 24);
    form->setHorizontalSpacing(20);
    form->setVerticalSpacing(16);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignTop | Qt::AlignHCenter);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
}

QWidget *SettingsWidget::createCenteredPage(QWidget *sectionContent)
{
    auto *page = new QWidget(m_sectionStack);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(sectionContent, 0, Qt::AlignHCenter);
    layout->addStretch();
    return page;
}

QWidget *SettingsWidget::createCameraSection()
{
    auto *formContainer = new QFrame(this);
    formContainer->setObjectName("SettingsFormContainer");
    formContainer->setMaximumWidth(820);
    formContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto *form = new QFormLayout(formContainer);
    configureFormLayout(form);

    ipEdit = new QLineEdit(formContainer);
    portEdit = new QLineEdit(formContainer);
    customCctvUserEdit = new QLineEdit(formContainer);
    customCctvPasswordEdit = new QLineEdit(formContainer);
    markInput(ipEdit);
    markInput(portEdit);
    markInput(customCctvUserEdit);
    markInput(customCctvPasswordEdit);

    rtspsOptionCheck = new QCheckBox(QStringLiteral("Use Secure RTSPS (Port 8322)"), formContainer);
    cctvOptionCheck = new QCheckBox(QStringLiteral("Use Custom CCTV URL"), formContainer);
    markCheck(rtspsOptionCheck);
    markCheck(cctvOptionCheck);

    ipEdit->setText(ConfigManager::instance().getCameraIp());
    portEdit->setText(ConfigManager::instance().getCameraPort());
    customCctvUserEdit->setText(ConfigManager::instance().getCustomCctvUsername());
    customCctvPasswordEdit->setText(ConfigManager::instance().getCustomCctvPassword());
    cctvOptionCheck->setChecked(ConfigManager::instance().getUseCustomCCTV());
    rtspsOptionCheck->setChecked(ConfigManager::instance().getUseRtsps());
    customCctvUserEdit->setPlaceholderText(QStringLiteral("admin"));
    customCctvPasswordEdit->setPlaceholderText(QStringLiteral("password"));
    customCctvPasswordEdit->setEchoMode(QLineEdit::Password);

    auto *cameraLabel = new QLabel(QStringLiteral("Camera IP:"), formContainer);
    auto *portLabel = new QLabel(QStringLiteral("RTSP Port:"), formContainer);
    auto *securityLabel = new QLabel(QStringLiteral("Security Mode:"), formContainer);
    auto *cctvLabel = new QLabel(QStringLiteral("CCTV Option:"), formContainer);
    auto *customUserLabel = new QLabel(QStringLiteral("Custom CCTV ID:"), formContainer);
    auto *customPasswordLabel = new QLabel(QStringLiteral("Custom CCTV Password:"), formContainer);
    auto *cctvHint = new QLabel(QStringLiteral("Custom mode uses: rtsp://<ID>:<PASSWORD>@<IP>:<PORT>/<ID>/H.264/media.smp"), formContainer);

    markLabel(cameraLabel, "label");
    markLabel(portLabel, "label");
    markLabel(securityLabel, "label");
    markLabel(cctvLabel, "label");
    markLabel(customUserLabel, "label");
    markLabel(customPasswordLabel, "label");
    markLabel(cctvHint, "hint");

    form->addRow(cameraLabel, ipEdit);
    form->addRow(portLabel, portEdit);
    form->addRow(securityLabel, rtspsOptionCheck);
    form->addRow(cctvLabel, cctvOptionCheck);
    form->addRow(customUserLabel, customCctvUserEdit);
    form->addRow(customPasswordLabel, customCctvPasswordEdit);
    form->addRow(QString(), cctvHint);

    const auto updateCustomCctvFieldsVisibility = [customUserLabel,
                                                   customPasswordLabel,
                                                   cctvHint,
                                                   this](bool enabled) {
        customUserLabel->setVisible(enabled);
        customCctvUserEdit->setVisible(enabled);
        customPasswordLabel->setVisible(enabled);
        customCctvPasswordEdit->setVisible(enabled);
        cctvHint->setVisible(enabled);
    };

    updateCustomCctvFieldsVisibility(cctvOptionCheck->isChecked());
    connect(cctvOptionCheck, &QCheckBox::toggled, formContainer, updateCustomCctvFieldsVisibility);

    return formContainer;
}

QWidget *SettingsWidget::createRobotSection()
{
    auto *formContainer = new QFrame(this);
    formContainer->setObjectName("SettingsFormContainer");
    formContainer->setMaximumWidth(820);
    formContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto *form = new QFormLayout(formContainer);
    configureFormLayout(form);

    robotIpEdit = new QLineEdit(formContainer);
    manualLinearSpin = new QDoubleSpinBox(formContainer);
    manualAngularSpin = new QDoubleSpinBox(formContainer);
    autoNavSpeedSpin = new QDoubleSpinBox(formContainer);
    markInput(robotIpEdit);
    markInput(manualLinearSpin);
    markInput(manualAngularSpin);
    markInput(autoNavSpeedSpin);

    manualControlCheck = new QCheckBox(QStringLiteral("Enable Manual Control (WASD)"), formContainer);
    markCheck(manualControlCheck);

    robotIpEdit->setText(ConfigManager::instance().getRobotHost());
    robotIpEdit->setPlaceholderText(QStringLiteral("172.20.26.8"));
    manualControlCheck->setChecked(ConfigManager::instance().getManualControl());

    manualLinearSpin->setDecimals(2);
    manualLinearSpin->setRange(0.0, 1.0);
    manualLinearSpin->setSingleStep(0.01);
    manualLinearSpin->setValue(ConfigManager::instance().getManualLinearX());

    manualAngularSpin->setDecimals(2);
    manualAngularSpin->setRange(0.0, 1.0);
    manualAngularSpin->setSingleStep(0.01);
    manualAngularSpin->setValue(ConfigManager::instance().getManualAngularZ());

    autoNavSpeedSpin->setDecimals(2);
    autoNavSpeedSpin->setRange(0.0, 1.0);
    autoNavSpeedSpin->setSingleStep(0.01);
    autoNavSpeedSpin->setValue(ConfigManager::instance().getAutoNavSpeed());

    auto *robotLabel = new QLabel(QStringLiteral("ROS2 Bridge Host:"), formContainer);
    auto *robotHint = new QLabel(QStringLiteral("Enter only the host/IP. WS becomes ws://<host>:9090 and RC RTSP becomes rtsp://<host>:9554/camera"), formContainer);
    auto *controlLabel = new QLabel(QStringLiteral("Control:"), formContainer);
    auto *linearLabel = new QLabel(QStringLiteral("Linear X:"), formContainer);
    auto *angularLabel = new QLabel(QStringLiteral("Angular Z:"), formContainer);
    auto *autoSpeedLabel = new QLabel(QStringLiteral("Auto Speed:"), formContainer);
    auto *controlHint = new QLabel(QStringLiteral("Set manual and autonomous speeds from 0.00 to 1.00."), formContainer);

    markLabel(robotLabel, "label");
    markLabel(robotHint, "hint");
    markLabel(controlLabel, "section");
    markLabel(linearLabel, "label");
    markLabel(angularLabel, "label");
    markLabel(autoSpeedLabel, "label");
    markLabel(controlHint, "hint");

    form->addRow(robotLabel, robotIpEdit);
    form->addRow(QString(), robotHint);
    form->addRow(controlLabel, manualControlCheck);
    form->addRow(linearLabel, manualLinearSpin);
    form->addRow(angularLabel, manualAngularSpin);
    form->addRow(autoSpeedLabel, autoNavSpeedSpin);
    form->addRow(QString(), controlHint);

    return formContainer;
}

void SettingsWidget::updateSectionTitle()
{
    if (!m_titleLabel) {
        return;
    }

    m_titleLabel->setText(m_currentSection == CameraSection
                              ? QStringLiteral("Camera Settings")
                              : QStringLiteral("Robot Car Settings"));
}
