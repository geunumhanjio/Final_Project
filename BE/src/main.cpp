#include <iostream>
#include <gst/gst.h>
#include <thread>
#include <memory>
#include "media/RtspProxy.hpp"
#include "network/VmsServer.hpp"
#include "network/RosClient.hpp"
#include "calibration/CalibrationManager.h"
#include "media/GstQualityMonitor.hpp"

int main(int argc, char *argv[]) {
    // Initialize GStreamer
    gst_init(&argc, &argv);

    // Register Custom Elements
    gst_quality_monitor_register(NULL);

    std::cout << "=================================================" << std::endl;
    std::cout << " 🚀 Proxy Server (Hybrid Architecture)" << std::endl;
    std::cout << "=================================================" << std::endl;

    // 1. Initialize Components
    RtspProxy proxy;
    CalibrationManager calibrator;
    
    // Perform initial mock calibration for testing
    calibrator.performCalibration();

    // 2. Network Setup (Boost.Asio)
    boost::asio::io_context ioc;
    
    // Create Servers/Clients
    // VMS listens on port 9000
    auto vmsServer = std::make_shared<VmsServer>(ioc, 9000);
    
    // ROS Client connects to localhost:9090 (standard rosbridge port)
    auto rosClient = std::make_shared<RosClient>(ioc);

    // 3. Logic Binding (The "Glue")
    vmsServer->setCoordinateCallback([&](double x, double y) {
        std::cout << "📩 [VMS -> Core] Received: (" << x << ", " << y << ")" << std::endl;
        
        // Transform coordinates
        cv::Point2f input((float)x, (float)y);
        cv::Point2f output = calibrator.apply(input);
        
        std::cout << "🧮 [Core -> Calibration] Transformed: (" << output.x << ", " << output.y << ")" << std::endl;

        // Send to ROS
        rosClient->publishGoal(output.x, output.y);
        std::cout << "📤 [Core -> ROS] Published Goal" << std::endl;
    });

    vmsServer->setCommandCallback([&](int sessionId, const std::string& cmd, int channel) {
        std::cout << "🎮 [VMS -> Core] Command: " << cmd << " for Ch: " << channel << " from Session " << sessionId << std::endl;
        
        if (cmd == "start") {
            if (proxy.startRecording(channel)) {
                std::cout << "✅ [Core] Recording started for Ch " << channel << std::endl;
            } else {
                std::cerr << "❌ [Core] Failed to start recording for Ch " << channel << std::endl;
            }
        } else if (cmd == "stop") {
            std::string filename = proxy.stopRecording(channel);
            if (!filename.empty()) {
                std::cout << "✅ [Core] Recording stopped. File saved: " << filename << std::endl;

                // Transfer file ONLY to the requesting session
                std::cout << "📤 [Core -> VMS] Sending to session " << sessionId << ": " << filename << std::endl;
                vmsServer->sendFileToClient(sessionId, filename);
                
            } else {
                std::cerr << "❌ [Core] Failed to stop recording (or not recording) for Ch " << channel << std::endl;
            }
        }
    });

    proxy.setStatsCallback([&](int channelId, const ChannelStats& stats) {
        vmsServer->broadcastStats(channelId, stats);
    });

    // 4. Start Services
    if (!proxy.initialize(8554)) {
        std::cerr << "❌ Failed to initialize RTSP Proxy." << std::endl;
        return -1;
    }
    
    // Start RTSPS (SSL)
    if (!proxy.startRTSPS(8322)) {
        std::cerr << "⚠️ Failed to start RTSPS Server." << std::endl;
    }
    
    proxy.start();

    vmsServer->start();
    rosClient->connect("192.168.0.237", "9090");

    // 5. Run Network IO in a background thread
    std::thread networkThread([&] {
        std::cout << "🌐 [Network] Event Loop Started" << std::endl;
        ioc.run();
    });
    networkThread.detach();

    std::cout << "✅ System Operational. Press Ctrl+C to stop." << std::endl;

    // 6. Run Main Loop (GStreamer Blocking)
    proxy.run();

    return 0;
}