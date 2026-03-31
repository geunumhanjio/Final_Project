#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QString>

namespace Constants {
    
    // Application constants
    namespace App {
        constexpr int DEFAULT_WIDTH = 1280;
        constexpr int DEFAULT_HEIGHT = 720;
        constexpr QStringView WINDOW_TITLE = u"누비고 프로그램";
    }
    
    // Robot control constants
    namespace Robot {
        constexpr double GOAL_DISTANCE_TOLERANCE_METERS = 0.20;
        constexpr double GOAL_LINEAR_SPEED_TOLERANCE = 0.05;
        constexpr double GOAL_ANGULAR_SPEED_TOLERANCE = 0.10;
        constexpr int REQUIRED_STABLE_SAMPLES = 3;
        constexpr double CAMERA_TILT_MIN = -30.0;
        constexpr double CAMERA_TILT_MAX = 30.0;
        constexpr double CAMERA_TILT_DELTA = 2.0;
        constexpr int INPUT_TIMER_INTERVAL_MS = 100;
        constexpr int CAMERA_TILT_TIMER_INTERVAL_MS = 50;
    }
    
    // Network and paths
    namespace Paths {
        constexpr QStringView GSTREAMER_DLL = u"gstreamer-1.0-0.dll";
        constexpr QStringView GSTREAMER_FOLDER = u"gstreamer";
        constexpr QStringView GSTREAMER_BIN_FOLDER = u"bin";
        constexpr QStringView GSTREAMER_PLUGIN_PATH = u"lib/gstreamer-1.0";
        constexpr QStringView GSTREAMER_SCANNER = u"libexec/gstreamer-1.0/gst-plugin-scanner.exe";
    }
    
    // Font resources
    namespace Fonts {
        constexpr QStringView PRETENDARD_REGULAR = u":/fonts/Pretendard-Regular.otf";
        constexpr QStringView PRETENDARD_MEDIUM = u":/fonts/Pretendard-Medium.otf";
        constexpr QStringView PRETENDARD_BOLD = u":/fonts/Pretendard-Bold.otf";
    }
    
    // SSL certificates
    namespace SSL {
        constexpr QStringView SERVER_CERT = u":/crt/env/server.crt";
    }
    
    // JSON keys
    namespace Json {
        constexpr QStringView POSITION = u"position";
        constexpr QStringView TRANSLATION = u"translation";
        constexpr QStringView POSE = u"pose";
        constexpr QStringView TRANSFORM = u"transform";
        constexpr QStringView MSG = u"msg";
        constexpr QStringView DATA = u"data";
        constexpr QStringView PAYLOAD = u"payload";
        constexpr QStringView X = u"x";
        constexpr QStringView Y = u"y";
        constexpr QStringView VELOCITY = u"velocity";
        constexpr QStringView LINEAR_X = u"linear_x";
        constexpr QStringView LINEAR = u"linear";
        constexpr QStringView ANGULAR_Z = u"angular_z";
        constexpr QStringView ANGULAR = u"angular";
        constexpr QStringView SPEED = u"speed";
        constexpr QStringView TWIST = u"twist";
        constexpr QStringView STATUS = u"status";
        constexpr QStringView STATE = u"state";
        constexpr QStringView RESULT = u"result";
        constexpr QStringView OUTCOME = u"outcome";
        constexpr QStringView GOAL_STATUS = u"goal_status";
        constexpr QStringView EVENT = u"event";
        constexpr QStringView MESSAGE = u"message";
        constexpr QStringView STATUS_CODE = u"status_code";
        constexpr QStringView CODE = u"code";
        constexpr QStringView RESULT_CODE = u"result_code";
    }
    
    // Success status values
    namespace Status {
        constexpr int SUCCEEDED = 3;
        constexpr int ABORTED = 4;
    }
    
    // GStreamer debug
    namespace GStreamer {
        constexpr QStringView DEBUG_CONFIG = u"1,libav:0";
    }
    
}

#endif // CONSTANTS_H
