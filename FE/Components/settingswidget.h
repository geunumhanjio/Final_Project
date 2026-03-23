#ifndef SETTINGSWIDGET_H
#define SETTINGSWIDGET_H

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QStackedWidget>
#include <QWidget>

class QFormLayout;

class SettingsWidget : public QWidget
{
    Q_OBJECT

public:
    enum Section {
        CameraSection = 0,
        RobotCarSection = 1
    };
    Q_ENUM(Section)

    explicit SettingsWidget(QWidget *parent = nullptr);

    Section currentSection() const;

public slots:
    void setSection(SettingsWidget::Section section);

signals:
    void autoNavSpeedApplyRequested(double speed);

private slots:
    void saveSettings();

private:
    static void markLabel(QLabel *label, const char *role);
    static void markInput(QWidget *widget);
    static void markCheck(QCheckBox *check);
    static void configureFormLayout(QFormLayout *form);

    QWidget *createCenteredPage(QWidget *sectionContent);
    QWidget *createCameraSection();
    QWidget *createRobotSection();
    void updateSectionTitle();

private:
    QLabel *m_titleLabel = nullptr;
    QStackedWidget *m_sectionStack = nullptr;
    Section m_currentSection = CameraSection;

    QLineEdit *ipEdit = nullptr;
    QLineEdit *portEdit = nullptr;
    QLineEdit *customCctvUserEdit = nullptr;
    QLineEdit *customCctvPasswordEdit = nullptr;
    QLineEdit *robotIpEdit = nullptr;
    QCheckBox *rtspsOptionCheck = nullptr;
    QCheckBox *cctvOptionCheck = nullptr;
    QCheckBox *manualControlCheck = nullptr;
    QDoubleSpinBox *manualLinearSpin = nullptr;
    QDoubleSpinBox *manualAngularSpin = nullptr;
    QDoubleSpinBox *autoNavSpeedSpin = nullptr;
};

#endif // SETTINGSWIDGET_H
