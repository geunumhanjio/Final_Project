cmake_minimum_required(VERSION 3.16)

project(VEDA_QT_1 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(VEDA_ENABLE_FRUC "Enable OpenCV-based FRUC generation" ON)
set(GST_ROOT "" CACHE PATH "Path to the GStreamer root directory")

# 1. Qt 라이브러리 찾기
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets WebSockets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets WebSockets Network)

# -------------------------------------------------------------------------
# 2. [수동 연결] GStreamer MinGW 라이브러리 직접 지정
# -------------------------------------------------------------------------

# 2. [수동 연결] GStreamer 라이브러리 경로 지정 (MSVC / MinGW 분기)
# -------------------------------------------------------------------------

if(MSVC)
    # MSVC용 GStreamer 경로 (기본 설치 경로)
    set(GST_ROOT_CANDIDATE_1 "C:/Program Files/gstreamer/1.0/msvc_x86_64")
    set(GST_ROOT_CANDIDATE_2 "C:/gstreamer/1.0/msvc_x86_64")
    set(GST_ROOT_CANDIDATE_3 "D:/gstreamer/1.0/msvc_x86_64") 
    
    if(EXISTS "${GST_ROOT_CANDIDATE_1}")
        set(GST_ROOT "${GST_ROOT_CANDIDATE_1}")
    elseif(EXISTS "${GST_ROOT_CANDIDATE_2}")
        set(GST_ROOT "${GST_ROOT_CANDIDATE_2}")
    elseif(EXISTS "${GST_ROOT_CANDIDATE_3}")
        set(GST_ROOT "${GST_ROOT_CANDIDATE_3}")
    else()
        message(WARNING "GStreamer MSVC paths not found in standard locations. Please set GST_ROOT manually.")
        set(GST_ROOT "C:/Program Files/gstreamer/1.0/msvc_x86_64") # Fallback to standard
    endif()
    
    message(STATUS "Selected GStreamer Root (MSVC): ${GST_ROOT}")

else()
    # MinGW용 GStreamer 경로
    set(GST_ROOT "C:/Program Files/gstreamer/1.0/mingw_x86_64")
    message(STATUS "Selected GStreamer Root (MinGW): ${GST_ROOT}")
endif()

set(_configured_gst_root "$CACHE{GST_ROOT}")
set(_gst_root_candidates "")

if(MSVC)
    if(DEFINED ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64} AND NOT "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}" STREQUAL "")
        list(APPEND _gst_root_candidates "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}")
    endif()
    if(DEFINED ENV{GSTREAMER_ROOT_X86_64} AND NOT "$ENV{GSTREAMER_ROOT_X86_64}" STREQUAL "")
        list(APPEND _gst_root_candidates "$ENV{GSTREAMER_ROOT_X86_64}")
    endif()
    list(APPEND _gst_root_candidates
        "C:/Program Files/gstreamer/1.0/msvc_x86_64"
        "C:/gstreamer/1.0/msvc_x86_64"
        "D:/gstreamer/1.0/msvc_x86_64"
    )
else()
    if(DEFINED ENV{GSTREAMER_1_0_ROOT_MINGW_X86_64} AND NOT "$ENV{GSTREAMER_1_0_ROOT_MINGW_X86_64}" STREQUAL "")
        list(APPEND _gst_root_candidates "$ENV{GSTREAMER_1_0_ROOT_MINGW_X86_64}")
    endif()
    if(DEFINED ENV{GSTREAMER_ROOT_X86_64} AND NOT "$ENV{GSTREAMER_ROOT_X86_64}" STREQUAL "")
        list(APPEND _gst_root_candidates "$ENV{GSTREAMER_ROOT_X86_64}")
    endif()
    list(APPEND _gst_root_candidates
        "C:/Program Files/gstreamer/1.0/mingw_x86_64"
        "C:/gstreamer/1.0/mingw_x86_64"
        "D:/gstreamer/1.0/mingw_x86_64"
    )
endif()

if(_configured_gst_root AND EXISTS "${_configured_gst_root}")
    set(GST_ROOT "${_configured_gst_root}")
else()
    foreach(_gst_root_candidate IN LISTS _gst_root_candidates)
        if(_gst_root_candidate AND EXISTS "${_gst_root_candidate}")
            set(GST_ROOT "${_gst_root_candidate}")
            break()
        endif()
    endforeach()
endif()

if(NOT GST_ROOT OR NOT EXISTS "${GST_ROOT}")
    message(FATAL_ERROR "GStreamer root was not found. Set GST_ROOT or the appropriate GSTREAMER_* environment variable before configuring.")
endif()

message(STATUS "Using GStreamer Root: ${GST_ROOT}")

set(OPENCV_ROOT "" CACHE PATH "Optional OpenCV root used for FRUC support")
set(OPENCV_BIN_DIR "")
set(VEDA_HAS_OPENCV OFF)

if(VEDA_ENABLE_FRUC)
    set(_opencv_dir_hints "")

    if(OPENCV_ROOT AND EXISTS "${OPENCV_ROOT}")
        list(APPEND _opencv_dir_hints
            "${OPENCV_ROOT}"
            "${OPENCV_ROOT}/build"
        )
    endif()

    if(DEFINED ENV{OPENCV_ROOT} AND NOT "$ENV{OPENCV_ROOT}" STREQUAL "")
        list(APPEND _opencv_dir_hints
            "$ENV{OPENCV_ROOT}"
            "$ENV{OPENCV_ROOT}/build"
        )
    endif()

    if(DEFINED ENV{OpenCV_DIR} AND NOT "$ENV{OpenCV_DIR}" STREQUAL "")
        list(APPEND _opencv_dir_hints
            "$ENV{OpenCV_DIR}"
            "$ENV{OpenCV_DIR}/build"
        )
    endif()

    list(APPEND _opencv_dir_hints
        "C:/opencv/build"
        "C:/OpenCV/build"
        "D:/opencv/build"
        "D:/OpenCV/build"
        "C:/tools/opencv/build"
        "D:/tools/opencv/build"
    )

    if(NOT OpenCV_DIR OR NOT EXISTS "${OpenCV_DIR}/OpenCVConfig.cmake")
        foreach(_opencv_dir_hint IN LISTS _opencv_dir_hints)
            if(_opencv_dir_hint AND EXISTS "${_opencv_dir_hint}/OpenCVConfig.cmake")
                set(OpenCV_DIR "${_opencv_dir_hint}" CACHE PATH "Directory containing OpenCVConfig.cmake" FORCE)

                if((NOT OPENCV_ROOT OR NOT EXISTS "${OPENCV_ROOT}") AND EXISTS "${_opencv_dir_hint}/x64")
                    set(OPENCV_ROOT "${_opencv_dir_hint}" CACHE PATH "Optional OpenCV root used for FRUC support" FORCE)
                endif()

                message(STATUS "Detected OpenCV configuration at: ${OpenCV_DIR}")
                break()
            endif()
        endforeach()
    endif()

    if(OPENCV_ROOT AND EXISTS "${OPENCV_ROOT}")
        list(PREPEND CMAKE_PREFIX_PATH "${OPENCV_ROOT}")
    endif()

    if(DEFINED ENV{OpenCV_DIR} AND NOT "$ENV{OpenCV_DIR}" STREQUAL "")
        list(PREPEND CMAKE_PREFIX_PATH "$ENV{OpenCV_DIR}")
    endif()

    if(OpenCV_DIR AND EXISTS "${OpenCV_DIR}")
        list(PREPEND CMAKE_PREFIX_PATH "${OpenCV_DIR}")
    endif()

    find_package(OpenCV QUIET COMPONENTS core imgproc video videoio)

    if(OpenCV_FOUND)
        set(VEDA_HAS_OPENCV ON)

        if(MSVC)
            set(_opencv_bin_suffix "x64/vc16/bin")
        else()
            set(_opencv_bin_suffix "x64/mingw/bin")
        endif()

        if(OPENCV_ROOT AND EXISTS "${OPENCV_ROOT}")
            set(OPENCV_BIN_DIR "${OPENCV_ROOT}/${_opencv_bin_suffix}")
        elseif(DEFINED OpenCV_DIR AND NOT "${OpenCV_DIR}" STREQUAL "")
            get_filename_component(_opencv_dir_abs "${OpenCV_DIR}" ABSOLUTE)
            set(_opencv_bin_candidates
                "${_opencv_dir_abs}/${_opencv_bin_suffix}"
                "${_opencv_dir_abs}/bin"
                "${_opencv_dir_abs}/../bin"
                "${_opencv_dir_abs}/../../${_opencv_bin_suffix}"
            )

            foreach(_opencv_bin_candidate IN LISTS _opencv_bin_candidates)
                if(EXISTS "${_opencv_bin_candidate}")
                    set(OPENCV_BIN_DIR "${_opencv_bin_candidate}")
                    break()
                endif()
            endforeach()
        endif()

        message(STATUS "OpenCV found. FRUC support enabled.")
        if(OPENCV_BIN_DIR)
            message(STATUS "OpenCV bin path: ${OPENCV_BIN_DIR}")
        endif()
    else()
        message(WARNING "OpenCV was not found. FRUC support will be disabled. Set OPENCV_ROOT or OpenCV_DIR to enable it.")
    endif()
else()
    message(STATUS "VEDA_ENABLE_FRUC=OFF. Building without OpenCV/FRUC support.")
endif()

# (1) 헤더 파일 경로 추가 (공통)
# (1) 헤더 파일 경로 추가 (공통)
include_directories(
    "${GST_ROOT}/include/gstreamer-1.0"
    "${GST_ROOT}/include/glib-2.0"
    "${GST_ROOT}/lib/glib-2.0/include"
    # [New] Modularized Paths
    "${CMAKE_CURRENT_SOURCE_DIR}/Views"
    "${CMAKE_CURRENT_SOURCE_DIR}/Components"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video"
    "${CMAKE_CURRENT_SOURCE_DIR}/Video/Gst"
    "${CMAKE_CURRENT_SOURCE_DIR}/Network"
    "${CMAKE_CURRENT_SOURCE_DIR}/Utils"
)

# (2) bin 경로 정의 (main.cpp에서 DLL 로딩용)
add_compile_definitions(GST_BIN_PATH="${GST_ROOT}/bin")
add_compile_definitions(OPENCV_BIN_PATH="${OPENCV_BIN_DIR}")

if(VEDA_HAS_OPENCV)
    include_directories("${OpenCV_INCLUDE_DIRS}")
    add_compile_definitions(VEDA_HAS_OPENCV=1)
endif()

# -------------------------------------------------------------------------

# 3. 프로젝트 소스 파일 (상대 경로 - FE 디렉터리 기준)
set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp mainwindow.h
        mainwindow_ui.cpp
        mainwindow_session.cpp
        Components/topbar.h Components/topbar.cpp
        Components/sidebar.h Components/sidebar.cpp
        Views/liveview.h Views/liveview.cpp
        Views/fullscreenview.h Views/fullscreenview.cpp
        Video/videowidget.h Video/videowidget.cpp
        Video/livevideowidget.h Video/livevideowidget.cpp
        Video/recordedvideowidget.h Video/recordedvideowidget.cpp
        Video/rtsppinger.h Video/rtsppinger.cpp
        Video/Gst/GstQualityMonitor.hpp Video/Gst/GstQualityMonitor.cpp
        Video/Gst/GstStatsCollector.hpp
        Components/videocard.h Components/videocard.cpp
        Components/slammapwidget.h Components/slammapwidget.cpp
        Network/streammanager.h Network/streammanager.cpp
        Network/authmanager.h Network/authmanager.cpp
        Network/rosbridgeclient.h Network/rosbridgeclient.cpp
        Network/cameracontrolclient.h Network/cameracontrolclient.cpp
        Views/playbackview.h Views/playbackview.cpp
        Utils/applicationinitializer.h Utils/applicationinitializer.cpp
        Utils/configmanager.h
        Utils/channelcatalog.h Utils/channelcatalog.cpp
        Utils/goaloverlaycontroller.h Utils/goaloverlaycontroller.cpp
        Utils/framelessconfirmdialog.h Utils/framelessconfirmdialog.cpp
        Utils/jsonutils.h Utils/jsonutils.cpp
        Components/settingswidget.h Components/settingswidget.cpp
        Components/logindialog.h Components/logindialog.cpp
        Components/osdwidget.h Components/osdwidget.cpp # [New]
)

if(VEDA_HAS_OPENCV)
    list(APPEND PROJECT_SOURCES
        Video/frucvideoprocessor.h Video/frucvideoprocessor.cpp
    )
endif()

if(WIN32)
    list(APPEND PROJECT_SOURCES
        app_icon.rc
    )
endif()

# 4. 실행 파일 생성
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(VEDA_QT_1 MANUAL_FINALIZATION ${PROJECT_SOURCES}
        Components/full_underbar.h Components/full_underbar.cpp
        resources.qrc)
else()
    if(ANDROID)
        add_library(VEDA_QT_1 SHARED ${PROJECT_SOURCES})
    else()
        add_executable(VEDA_QT_1 ${PROJECT_SOURCES})
    endif()
endif()

# 5. 라이브러리 연결
target_link_libraries(VEDA_QT_1 PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::WebSockets
    Qt${QT_VERSION_MAJOR}::Network
)

if(MSVC)
    target_link_options(VEDA_QT_1 PRIVATE
        $<$<CONFIG:Debug>:/INCREMENTAL:NO>
        $<$<CONFIG:RelWithDebInfo>:/INCREMENTAL:NO>
    )
endif()

if(VEDA_HAS_OPENCV)
    target_link_libraries(VEDA_QT_1 PRIVATE ${OpenCV_LIBS})
endif()


# -------------------------------------------------------------------------
# [수동 연결] 라이브러리 파일 직접 링크 (MSVC vs MinGW)
# -------------------------------------------------------------------------

if(MSVC)
    # MSVC는 .lib 파일을 링크
    target_link_libraries(VEDA_QT_1 PRIVATE
        "${GST_ROOT}/lib/gstreamer-1.0.lib"
        "${GST_ROOT}/lib/gstvideo-1.0.lib"
        "${GST_ROOT}/lib/gstrtp-1.0.lib"
        "${GST_ROOT}/lib/gstrtsp-1.0.lib"
        "${GST_ROOT}/lib/gobject-2.0.lib"
        "${GST_ROOT}/lib/glib-2.0.lib"
        "${GST_ROOT}/lib/gstapp-1.0.lib"
        "${GST_ROOT}/lib/gstbase-1.0.lib"
    )
else()
    # MinGW는 .dll.a 파일을 링크
    target_link_libraries(VEDA_QT_1 PRIVATE
        "${GST_ROOT}/lib/libgstreamer-1.0.dll.a"
        "${GST_ROOT}/lib/libgstvideo-1.0.dll.a"
        "${GST_ROOT}/lib/libgstrtp-1.0.dll.a"
        "${GST_ROOT}/lib/libgstrtsp-1.0.dll.a"
        "${GST_ROOT}/lib/libgobject-2.0.dll.a"
        "${GST_ROOT}/lib/libglib-2.0.dll.a"
        "${GST_ROOT}/lib/libgstapp-1.0.dll.a"
        "${GST_ROOT}/lib/libgstbase-1.0.dll.a"
    )
endif()
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
