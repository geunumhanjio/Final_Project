#include "full_underbar.h"
#include <QFrame>

#include <QStyleOption>
#include <QPainter>
#include <QDateTime>

FullUnderBar::FullUnderBar(QWidget *parent) : QWidget(parent)
{
    this->setFixedHeight(50); // Thinner bar
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setAttribute(Qt::WA_StyledBackground, true); // Ensure QSS background paints
    
    // Main styling
    this->setObjectName("controlBar");
    this->setStyleSheet(
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
        "   font-size: 13px; font-weight: 500; font-family: 'Segoe UI', sans-serif;"
        "}"
        "QPushButton:hover { "
        "   background-color: rgba(255, 255, 255, 0.1); "
        "   border-radius: 8px; "
        "   color: #FFFFFF; "
        "}"
        "QPushButton:checked { "
        "   color: #0EA5E9; font-weight: bold;"
        "   background-color: rgba(14, 165, 233, 0.1);"
        "}"
    );

    // EHBox *layout = new EHBox(this); // typo removed
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 5, 10, 5);
    mainLayout->setContentsMargins(10, 5, 10, 5);
    mainLayout->setSpacing(0); // We use dividers

    // [New] Playback Controls Container
    playbackContainer = new QWidget(this);
    playbackContainer->setObjectName("FS_PlaybackContainer"); // [New]
    QHBoxLayout *pbLayout = new QHBoxLayout(playbackContainer);
    pbLayout->setContentsMargins(0, 0, 0, 0);
    pbLayout->setSpacing(8);

    btnPlayPause = new QPushButton("⏯", playbackContainer);
    btnPlayPause->setFixedSize(36, 36);
    btnPlayPause->setObjectName("playBtn");

    // [New] Record Button (Visible Only in Live Mode)
    btnRecord = new QPushButton("REC", this); // Added to main layout, not playback container
    btnRecord->setCheckable(true);
    btnRecord->setFixedSize(60, 36);
    btnRecord->setObjectName("recBtn");
    btnRecord->setStyleSheet(
        "#recBtn { color: #EF4444; border: 1px solid #EF4444; border-radius: 6px; font-weight: bold; background: transparent; }"
        "#recBtn:checked { background-color: #EF4444; color: white; }"
        "#recBtn:hover { background-color: rgba(239, 68, 68, 0.1); }"
    );

    btnSkipBackward = new QPushButton("⏪ -5s", playbackContainer);
    btnSkipBackward->setFixedWidth(60);

    btnSkipForward = new QPushButton("+5s ⏩", playbackContainer);
    btnSkipForward->setFixedWidth(60);

    seekSlider = new QSlider(Qt::Horizontal, playbackContainer);
    seekSlider->setObjectName("FS_SeekSlider"); // [New]
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

    // Initial Hide
    playbackContainer->setVisible(false);
    m_isFileMode = false;

    // Setup Buttons with Unicode Icons
    btnZoomIn = new QPushButton("⊕ Zoom Center", this);
    btnZoomOut = new QPushButton("⊖ Zoom Out", this);
    btnRectZoom = new QPushButton("⛶ Box Zoom", this);
    btnRectZoom->setCheckable(true);
    
    btnControlMode = new QPushButton("🎯 조종모드", this);
    btnControlMode->setCheckable(true);
    btnControlMode->setObjectName("controlModeBtn");
    btnControlMode->setStyleSheet(
        "#controlModeBtn { color: #F59E0B; border: 1px solid #F59E0B; border-radius: 6px; font-weight: bold; background: transparent; }"
        "#controlModeBtn:checked { background-color: #F59E0B; color: white; }"
        "#controlModeBtn:hover { background-color: rgba(245, 158, 11, 0.1); }"
    );
    
    btnResetZoom = new QPushButton("⟲ Reset", this);
    btnResetZoom->setObjectName("resetBtn");
    btnResetZoom->setStyleSheet(
        "#resetBtn { "
        "   background-color: #0EA5E9; color: white; border-radius: 8px; font-weight: bold; "
        "}"
        "#resetBtn:hover { background-color: #38BDF8; }"
    );

    // Connections
    connect(btnZoomIn, &QPushButton::clicked, this, &FullUnderBar::reqZoomIn);
    connect(btnZoomOut, &QPushButton::clicked, this, &FullUnderBar::reqZoomOut);
    connect(btnRectZoom, &QPushButton::toggled, this, &FullUnderBar::reqRectZoom);
    connect(btnResetZoom, &QPushButton::clicked, this, &FullUnderBar::reqResetZoom);
    connect(btnControlMode, &QPushButton::toggled, this, &FullUnderBar::reqControlMode);

    // [New] Record Connection
    connect(btnRecord, &QPushButton::toggled, [this](bool checked){
        if (checked) btnRecord->setText("STOP");
        else btnRecord->setText("REC");
        emit reqRecord(checked);
    });

    // [New] Playback Connections
    connect(btnPlayPause, &QPushButton::clicked, this, &FullUnderBar::reqPlayPause);
    connect(btnSkipBackward, &QPushButton::clicked, this, &FullUnderBar::reqSkipBackward);
    connect(btnSkipForward, &QPushButton::clicked, this, &FullUnderBar::reqSkipForward);
    connect(seekSlider, &QSlider::sliderReleased, [this](){
        emit reqSeek(seekSlider->value());
    });
    // Optional: Seek while dragging
    // connect(seekSlider, &QSlider::sliderMoved, this, &FullUnderBar::reqSeek); 

    // Add to layout with dividers
    mainLayout->addWidget(playbackContainer);
    mainLayout->addWidget(btnRecord); // [New]
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

// Helper for divider
QFrame* FullUnderBar::createDivider() {
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
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void FullUnderBar::setRectButtonMode(int state)
{
    btnRectZoom->blockSignals(true);
    switch (state) {
    case 0:
        btnRectZoom->setText("⛶ Box Zoom");
        btnRectZoom->setChecked(false);
        break;
    case 1:
        btnRectZoom->setText("Cancel");
        btnRectZoom->setChecked(true);
        break;
    case 2:
        // 확대된 상태여도 다시 확대할 수 있도록 네모 버튼 원복
        btnRectZoom->setText("⛶ Box Zoom");
        btnRectZoom->setChecked(false);
        break;
    }
    btnRectZoom->blockSignals(false);
}

void FullUnderBar::setMode(bool isFile)
{
    m_isFileMode = isFile;
    m_isFileMode = isFile;
    playbackContainer->setVisible(isFile);
    btnRecord->setVisible(!isFile); // Show Record button only in Live mode
    btnControlMode->setVisible(!isFile); // Show Control Mode button only in Live mode
    
    // Reset Record Button state when switching modes
    if (isFile && btnRecord->isChecked()) {
        btnRecord->setChecked(false); // Stop if switching to file (shouldn't happen usually)
    }
}

void FullUnderBar::updateTime(qint64 currentMs, qint64 totalMs)
{
    if (totalMs <= 0) return;
    
    // Prevent slider update while user is dragging
    if (!seekSlider->isSliderDown()) {
        int val = (currentMs * 1000) / totalMs;
        seekSlider->setValue(val);
    }
    
    QTime current = QTime::fromMSecsSinceStartOfDay(currentMs);
    QTime total = QTime::fromMSecsSinceStartOfDay(totalMs);
    // Format: mm:ss
    timeLabel->setText(QString("%1 / %2")
        .arg(current.toString("mm:ss"))
        .arg(total.toString("mm:ss")));
}

void FullUnderBar::setPlaying(bool isPlaying)
{
    btnPlayPause->setText(isPlaying ? "⏸" : "⏯");
}


