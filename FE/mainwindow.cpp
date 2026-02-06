#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QCloseEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("CCTV 통합 관제 시스템 - 근엄한조");
    this->resize(1280, 720);
    initUI();
    initConnections();
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::initUI()
{
    this->setStyleSheet("QMainWindow { background-color: #222222; }");
    m_topBar = new TopBar(this);
    this->setMenuWidget(m_topBar);
    m_sidebar = new Sidebar("채널 목록", this);
    this->addDockWidget(Qt::LeftDockWidgetArea, m_sidebar);
    m_centralStack = new QStackedWidget(this);
    this->setCentralWidget(m_centralStack);

    m_livePage = new LiveView(this);
    m_playbackPage = new QLabel("기록 화면", this);
    m_settingsPage = new QLabel("설정 화면", this);
    m_fullPage = new FullScreenView(this);

    m_playbackPage->setStyleSheet("color:white; font-size:20px;");
    m_settingsPage->setStyleSheet("color:white; font-size:20px;");
    m_playbackPage->setAlignment(Qt::AlignCenter);
    m_settingsPage->setAlignment(Qt::AlignCenter);

    m_centralStack->addWidget(m_livePage);
    m_centralStack->addWidget(m_playbackPage);
    m_centralStack->addWidget(m_settingsPage);
    m_centralStack->addWidget(m_fullPage);
}

void MainWindow::initConnections()
{
    connect(m_topBar, &TopBar::sidebarToggled, [=](){ m_sidebar->setVisible(!m_sidebar->isVisible()); });
    connect(m_topBar, &TopBar::modeChanged, m_centralStack, &QStackedWidget::setCurrentIndex);
    connect(m_sidebar, &Sidebar::channelStateChanged, m_livePage, &LiveView::setChannelVisible);

    connect(m_livePage, &LiveView::requestFullScreen, [=](int index, QString url){
        if (index < 4 && !url.isEmpty()) {
            m_fullPage->play(url, index);
            m_centralStack->setCurrentWidget(m_fullPage);
        } else {
            m_centralStack->setCurrentWidget(m_fullPage);
        }
    });

    connect(m_fullPage, &FullScreenView::closeRequested, [=](){
        m_fullPage->stop();
        m_centralStack->setCurrentWidget(m_livePage);
    });
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_livePage) m_livePage->stopAll();
    if (m_fullPage) m_fullPage->stop();
    event->accept();
}
