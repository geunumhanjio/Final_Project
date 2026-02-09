cmake_minimum_required(VERSION 3.16)

project(VEDA_QT_1 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Qt 라이브러리 찾기
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

# -------------------------------------------------------------------------
# 2. [수동 연결] GStreamer MinGW 라이브러리 직접 지정
# -------------------------------------------------------------------------

# 사용자 설치 경로 (정확해야 합니다!)
set(GST_ROOT "C:/Program Files/gstreamer/1.0/mingw_x86_64")

# (1) 헤더 파일 경로 추가 (컴파일용)
include_directories(
    "${GST_ROOT}/include/gstreamer-1.0"
    "${GST_ROOT}/include/glib-2.0"
    "${GST_ROOT}/lib/glib-2.0/include"
)

# (2) bin 경로 정의 (main.cpp에서 DLL 로딩용)
add_compile_definitions(GST_BIN_PATH="${GST_ROOT}/bin")

# -------------------------------------------------------------------------

# 3. 프로젝트 소스 파일 (상대 경로 - FE 디렉터리 기준)
set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp mainwindow.h mainwindow.ui
        topbar.h topbar.cpp
        sidebar.h sidebar.cpp
        liveview.h liveview.cpp
        fullscreenview.h fullscreenview.cpp
        videowidget.h videowidget.cpp
        videocard.h videocard.cpp
)

# 4. 실행 파일 생성
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(VEDA_QT_1 MANUAL_FINALIZATION ${PROJECT_SOURCES}
        full_underbar.h full_underbar.cpp)
else()
    if(ANDROID)
        add_library(VEDA_QT_1 SHARED ${PROJECT_SOURCES})
    else()
        add_executable(VEDA_QT_1 ${PROJECT_SOURCES})
    endif()
endif()

# 5. 라이브러리 연결
target_link_libraries(VEDA_QT_1 PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

# -------------------------------------------------------------------------
# [수동 연결] 라이브러리 파일 직접 링크 (순서 중요)
# -------------------------------------------------------------------------
target_link_libraries(VEDA_QT_1 PRIVATE
    "${GST_ROOT}/lib/libgstreamer-1.0.dll.a"
    "${GST_ROOT}/lib/libgstvideo-1.0.dll.a"
    "${GST_ROOT}/lib/libgobject-2.0.dll.a"
    "${GST_ROOT}/lib/libglib-2.0.dll.a"
    "${GST_ROOT}/lib/libgstapp-1.0.dll.a"
    "${GST_ROOT}/lib/libgstbase-1.0.dll.a"
)
# -------------------------------------------------------------------------

# 6. 번들 설정 (기본값)
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.VEDA_QT_1)
endif()

set_target_properties(VEDA_QT_1 PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS VEDA_QT_1
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(VEDA_QT_1)
endif()
