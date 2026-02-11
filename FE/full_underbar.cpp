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
    QHBoxLayout *pbLayout = new QHBoxLayout(playbackContainer);
    pbLayout->setContentsMargins(0, 0, 0, 0);
    pbLayout->setSpacing(8);

    btnPlayPause = new QPushButton("⏯", playbackContainer);
    btnPlayPause->setFixedSize(36, 36);
    btnPlayPause->setObjectName("playBtn");

    btnSkipBackward = new QPushButton("⏪ -5s", playbackContainer);
    btnSkipBackward->setFixedWidth(60);

    btnSkipForward = new QPushButton("+5s ⏩", playbackContainer);
    btnSkipForward->setFixedWidth(60);

    seekSlider = new QSlider(Qt::Horizontal, playbackContainer);
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
    mainLayout->addWidget(createDivider());
    mainLayout->addWidget(btnZoomIn);
    mainLayout->addWidget(createDivider());
    mainLayout->addWidget(btnZoomOut);
    mainLayout->addWidget(createDivider());
    mainLayout->addWidget(btnRectZoom);
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
    playbackContainer->setVisible(isFile);
    // Adjust logic if needed (e.g., hide zoom controls if play mode? - keeping both for now)
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

void FullUnderBar::updateTheme(bool isDark)
{
    if (isDark) {
        // Dark Mode (Original)
        this->setStyleSheet(
            "#controlBar { "
            "   background-color: rgba(15, 23, 42, 0.6); " // More transparent (0.8 -> 0.6)
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
        
        // Reset Button Dark
        btnResetZoom->setStyleSheet(
            "#resetBtn { "
            "   background-color: #0EA5E9; color: white; border-radius: 8px; font-weight: bold; "
            "}"
            "#resetBtn:hover { background-color: #38BDF8; }"
        );

        // [New] Playback Styling Dark
        playbackContainer->setStyleSheet("background: transparent;");
        seekSlider->setStyleSheet(
            "QSlider::groove:horizontal { border: 1px solid #334155; height: 6px; background: #1E293B; margin: 2px 0; border-radius: 3px; }"
            "QSlider::handle:horizontal { background: #0EA5E9; border: 1px solid #0EA5E9; width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; }"
            "QSlider::handle:horizontal:hover { background: #38BDF8; }"
        );
        timeLabel->setStyleSheet("color: #E2E8F0; font-family: monospace; font-weight: bold;");
    } else {
        // Light Mode (White/Orange)
        // Background: White/60 (adjusted for transparency)
        this->setStyleSheet(
            "#controlBar { "
            "   background-color: rgba(255, 255, 255, 0.60); " // More transparent (0.85 -> 0.60)
            "   border: 1px solid rgba(0, 0, 0, 0.1); "
            "   border-radius: 12px; "
            "}"
            "QPushButton { "
            "   background-color: transparent; "
            "   color: #374151; " // Gray 700 (Lighter/Brighter than Black, visible on White)
            "   border: none; "
            "   padding: 8px 15px; "
            "   font-size: 13px; font-weight: 500; font-family: 'Segoe UI', sans-serif;"
            "}"
            "QPushButton:hover { "
            "   background-color: rgba(255, 255, 255, 1.0); " // White hover
            "   border-radius: 8px; "
            "   color: #F98006; " // Primary Orange
            "}"
            "QPushButton:checked { "
            "   color: #F98006; font-weight: bold;"
            "   background-color: rgba(249, 128, 6, 0.1);"
            "}"
        );

        // Reset Button Light (Primary Orange)
        btnResetZoom->setStyleSheet(
            "#resetBtn { "
            "   background-color: #F98006; color: white; border-radius: 8px; font-weight: bold; "
            "}"
            "#resetBtn:hover { background-color: #FB923C; }" // Orange-400
        );
        
        // [New] Playback Styling Light
        playbackContainer->setStyleSheet("background: transparent;");
        seekSlider->setStyleSheet(
            "QSlider::groove:horizontal { border: 1px solid #CBD5E1; height: 6px; background: #E2E8F0; margin: 2px 0; border-radius: 3px; }"
            "QSlider::handle:horizontal { background: #F98006; border: 1px solid #F98006; width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; }"
            "QSlider::handle:horizontal:hover { background: #FB923C; }"
        );
        timeLabel->setStyleSheet("color: #334155; font-family: monospace; font-weight: bold;");
    }
}
