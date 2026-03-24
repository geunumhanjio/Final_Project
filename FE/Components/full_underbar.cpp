#include "full_underbar.h"

#include <QDateTime>
#include <QFrame>
#include <QPainter>
#include <QStyleOption>

FullUnderBar::FullUnderBar(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(50);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("controlBar");
    setStyleSheet(
        "#controlBar { "
        "   background-color: rgba(15, 23, 42, 0.8); "
        "   border: 1px solid rgba(255, 255, 255, 0.1); "
        "   border-radius: 12px; "
        "}"
        "QPushButton { "
        "   background-color: transparent; "
        "   color: #CBD5E1; "
        "   border: none; "
        "   padding: 8px 15px; "
        "   font-size: 13px; font-weight: 500; font-family: 'Pretendard';"
        "}"
        "QPushButton:hover { "
        "   background-color: rgba(255, 255, 255, 0.1); "
        "   border-radius: 8px; "
        "   color: #FFFFFF; "
        "}"
        "QPushButton:checked { "
        "   color: #0EA5E9; font-weight: bold;"
        "   background-color: rgba(14, 165, 233, 0.1);"
        "}");

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 5, 10, 5);
    mainLayout->setSpacing(0);

    playbackContainer = new QWidget(this);
    playbackContainer->setObjectName("FS_PlaybackContainer");
    QHBoxLayout *pbLayout = new QHBoxLayout(playbackContainer);
    pbLayout->setContentsMargins(0, 0, 0, 0);
    pbLayout->setSpacing(8);

    btnPlayPause = new QPushButton("Play", playbackContainer);
    btnPlayPause->setFixedSize(52, 36);
    btnPlayPause->setObjectName("playBtn");

    btnRecord = new QPushButton("REC", this);
    btnRecord->setCheckable(true);
    btnRecord->setFixedSize(60, 36);
    btnRecord->setObjectName("recBtn");
    btnRecord->setStyleSheet(
        "#recBtn { color: #EF4444; border: 1px solid #EF4444; border-radius: 6px; font-weight: bold; background: transparent; }"
        "#recBtn:checked { background-color: #EF4444; color: white; }"
        "#recBtn:hover { background-color: rgba(239, 68, 68, 0.1); }");

    btnSkipBackward = new QPushButton("-5s", playbackContainer);
    btnSkipBackward->setFixedWidth(60);

    btnSkipForward = new QPushButton("+5s", playbackContainer);
    btnSkipForward->setFixedWidth(60);

    seekSlider = new QSlider(Qt::Horizontal, playbackContainer);
    seekSlider->setObjectName("FS_SeekSlider");
    seekSlider->setRange(0, 1000);
    seekSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    timeLabel = new QLabel("00:00 / 00:00", playbackContainer);
    timeLabel->setAlignment(Qt::AlignCenter);
    timeLabel->setFixedWidth(100);
    timeLabel->setObjectName("timeLabel");

    pbLayout->addWidget(btnSkipBackward);
    pbLayout->addWidget(btnPlayPause);
    pbLayout->addWidget(btnSkipForward);
    pbLayout->addWidget(seekSlider);
    pbLayout->addWidget(timeLabel);

    playbackContainer->setVisible(false);
    m_isFileMode = false;

    btnZoomIn = new QPushButton("Zoom Center", this);
    btnZoomOut = new QPushButton("Zoom Out", this);
    btnRectZoom = new QPushButton("Box Zoom", this);
    btnRectZoom->setCheckable(true);

    btnControlMode = new QPushButton("Start Control", this);
    btnControlMode->setCheckable(true);
    btnControlMode->setObjectName("controlModeBtn");
    btnControlMode->setStyleSheet(
        "#controlModeBtn { color: #F59E0B; border: 1px solid #F59E0B; border-radius: 6px; font-weight: bold; background: transparent; }"
        "#controlModeBtn:checked { background-color: #F59E0B; color: white; }"
        "#controlModeBtn:hover { background-color: rgba(245, 158, 11, 0.1); }");

    btnResetZoom = new QPushButton("Reset", this);
    btnResetZoom->setObjectName("resetBtn");
    btnResetZoom->setStyleSheet(
        "#resetBtn { background-color: #0EA5E9; color: white; border-radius: 8px; font-weight: bold; }"
        "#resetBtn:hover { background-color: #38BDF8; }");

    connect(btnZoomIn, &QPushButton::clicked, this, &FullUnderBar::reqZoomIn);
    connect(btnZoomOut, &QPushButton::clicked, this, &FullUnderBar::reqZoomOut);
    connect(btnRectZoom, &QPushButton::toggled, this, &FullUnderBar::reqRectZoom);
    connect(btnResetZoom, &QPushButton::clicked, this, &FullUnderBar::reqResetZoom);
    connect(btnControlMode, &QPushButton::toggled, this, [this](bool checked) {
        updateControlModeButtonText(checked);
        emit reqControlMode(checked);
    });

    connect(btnRecord, &QPushButton::toggled, [this](bool checked) {
        btnRecord->setText(checked ? "STOP" : "REC");
        emit reqRecord(checked);
    });

    connect(btnPlayPause, &QPushButton::clicked, this, &FullUnderBar::reqPlayPause);
    connect(btnSkipBackward, &QPushButton::clicked, this, &FullUnderBar::reqSkipBackward);
    connect(btnSkipForward, &QPushButton::clicked, this, &FullUnderBar::reqSkipForward);
    connect(seekSlider, &QSlider::sliderReleased, [this]() {
        emit reqSeek(seekSlider->value());
    });

    mainLayout->addWidget(playbackContainer);
    mainLayout->addWidget(btnRecord);
    mainLayout->addWidget(createDivider());
    mainLayout->addWidget(createDivider());
    mainLayout->addWidget(btnZoomIn);
    mainLayout->addWidget(createDivider());
    mainLayout->addWidget(btnZoomOut);
    mainLayout->addWidget(createDivider());
    mainLayout->addWidget(btnRectZoom);
    mainLayout->addWidget(createDivider());
    mainLayout->addWidget(btnControlMode);
    mainLayout->addWidget(createDivider());
    mainLayout->addWidget(btnResetZoom);
}

QFrame *FullUnderBar::createDivider()
{
    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::VLine);
    line->setFixedSize(1, 20);
    line->setStyleSheet("background-color: rgba(255, 255, 255, 0.1); border: none;");
    return line;
}

void FullUnderBar::paintEvent(QPaintEvent *)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
}

void FullUnderBar::setRectButtonMode(int state)
{
    btnRectZoom->blockSignals(true);
    switch (state) {
    case 0:
        btnRectZoom->setText("Box Zoom");
        btnRectZoom->setChecked(false);
        break;
    case 1:
        btnRectZoom->setText("Cancel");
        btnRectZoom->setChecked(true);
        break;
    case 2:
        btnRectZoom->setText("Box Zoom");
        btnRectZoom->setChecked(false);
        break;
    }
    btnRectZoom->blockSignals(false);
}

void FullUnderBar::setMode(bool isFile)
{
    m_isFileMode = isFile;
    playbackContainer->setVisible(isFile);
    btnRecord->setVisible(!isFile);
    btnControlMode->setVisible(!isFile);

    if (isFile && btnRecord->isChecked()) {
        btnRecord->setChecked(false);
    }
}

void FullUnderBar::updateTime(qint64 currentMs, qint64 totalMs)
{
    if (totalMs <= 0) {
        return;
    }

    if (!seekSlider->isSliderDown()) {
        const int val = (currentMs * 1000) / totalMs;
        seekSlider->setValue(val);
    }

    const QTime current = QTime::fromMSecsSinceStartOfDay(currentMs);
    const QTime total = QTime::fromMSecsSinceStartOfDay(totalMs);
    timeLabel->setText(QString("%1 / %2").arg(current.toString("mm:ss"), total.toString("mm:ss")));
}

void FullUnderBar::setPlaying(bool isPlaying)
{
    btnPlayPause->setText(isPlaying ? "Pause" : "Play");
}

void FullUnderBar::setControlModeAvailable(bool available)
{
    btnControlMode->setEnabled(available);
    if (!available && btnControlMode->isChecked()) {
        btnControlMode->blockSignals(true);
        btnControlMode->setChecked(false);
        btnControlMode->blockSignals(false);
    }
    updateControlModeButtonText(btnControlMode->isChecked());
}

void FullUnderBar::setControlModeChecked(bool checked)
{
    if (btnControlMode->isChecked() == checked) {
        updateControlModeButtonText(checked);
        return;
    }

    btnControlMode->blockSignals(true);
    btnControlMode->setChecked(checked);
    btnControlMode->blockSignals(false);
    updateControlModeButtonText(checked);
}

void FullUnderBar::updateControlModeButtonText(bool checked)
{
    btnControlMode->setText(checked ? QStringLiteral("Stop Control")
                                    : QStringLiteral("Start Control"));
}
