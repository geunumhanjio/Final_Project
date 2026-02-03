/**
 * @file mainwindow.cpp
 * @brief 메인 윈도우 구현. 모듈 조립 및 이벤트 연결.
 */
#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("CCTV 통합 관제 시스템 - 근엄한조");
    this->resize(1280, 720);

    initUI();
    initConnections();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initUI()
{
    // 전체 배경을 어둡게 설정 (빈 공간이 흰색으로 나오는 것 방지)
    this->setStyleSheet("QMainWindow { background-color: #222222; }");

    // 1. 상단바 생성 및 메뉴 위젯으로 등록
    m_topBar = new TopBar(this);
    this->setMenuWidget(m_topBar);

    // 2. 사이드바 생성 및 Dock 영역에 추가
    m_sidebar = new Sidebar("채널 목록", this);
    this->addDockWidget(Qt::LeftDockWidgetArea, m_sidebar);

    // 3. 중앙 페이지 컨테이너 생성
    m_centralStack = new QStackedWidget(this);
    this->setCentralWidget(m_centralStack);

    // 4. 각 페이지 생성
    m_livePage = new LiveView(this);
    m_playbackPage = new QLabel("기록 화면 (준비중)", this);
    m_settingsPage = new QLabel("설정 화면 (준비중)", this);
    m_fullPage = new FullScreenView(this);

    // 임시 페이지 스타일
    m_playbackPage->setAlignment(Qt::AlignCenter);
    m_playbackPage->setStyleSheet("color:white; font-size:20px;");
    m_settingsPage->setAlignment(Qt::AlignCenter);
    m_settingsPage->setStyleSheet("color:white; font-size:20px;");

    // 페이지 추가 (인덱스 순서대로)
    m_centralStack->addWidget(m_livePage);     // 0: 라이브
    m_centralStack->addWidget(m_playbackPage); // 1: 기록
    m_centralStack->addWidget(m_settingsPage); // 2: 설정
    m_centralStack->addWidget(m_fullPage);     // 3: 전체화면
}

void MainWindow::initConnections()
{
    // [TopBar -> 기타]
    // 햄버거 버튼 -> 사이드바 열기/닫기
    connect(m_topBar, &TopBar::sidebarToggled, [=](){
        m_sidebar->setVisible(!m_sidebar->isVisible());
    });
    // 모드 버튼 -> 중앙 페이지 전환
    connect(m_topBar, &TopBar::modeChanged, m_centralStack, &QStackedWidget::setCurrentIndex);

    // [Sidebar -> LiveView]
    // 채널 체크/해제 -> 해당 화면 보이기/숨기기
    connect(m_sidebar, &Sidebar::channelStateChanged, m_livePage, &LiveView::setChannelVisible);

    // [LiveView -> FullScreen]
    // 화면 더블 클릭 -> 전체 화면 모드로 전환
    connect(m_livePage, &LiveView::requestFullScreen, [=](int index){
        m_fullPage->setContent(index);
        m_centralStack->setCurrentWidget(m_fullPage);
    });

    // [FullScreen -> LiveView]
    // 닫기 버튼/더블 클릭 -> 라이브 화면으로 복귀
    connect(m_fullPage, &FullScreenView::closeRequested, [=](){
        m_centralStack->setCurrentWidget(m_livePage);
    });
}
