#ifndef RTSP_PROXY_HPP
#define RTSP_PROXY_HPP

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <memory>
#include <functional>
#include "media/GstStatsCollector.hpp"

// Forward declaration
struct ChannelContext;

struct ChannelStats {
    double fps;
    double bitrate_kbps;
    double proxy_latency_ms;

    // RTCP stats
    RtpQualityMetrics rtp;
};

using OnStatsUpdate = std::function<void(int channelId, const ChannelStats& stats)>;

class RtspProxy {
public:
    RtspProxy();
    ~RtspProxy();

    // Initialize GStreamer and RTSP Server
    bool initialize(int port = 8554);

    // Start all receiver threads and the RTSP server
    void start();

    // Stop the server and threads
    void stop();

    // Recording Control
    // Returns true if started successfully
    bool startRecording(int channelId);
    
    // Returns the filename of the saved video, or empty string if failed
    std::string stopRecording(int channelId);

    // Main GStreamer loop (blocking) - typically called by main()
    void run();

    void setStatsCallback(OnStatsUpdate cb) { statsCb = cb; }
    void triggerStatsCallback(int channelId, const ChannelStats& stats) { if (statsCb) statsCb(channelId, stats); }

private:
    void setupChannels();
    static void runReceiverThread(ChannelContext* ctx);
    static void mediaConfigure(GstRTSPMediaFactory* factory, GstRTSPMedia* media, gpointer user_data);

    // Helper to start the receiver thread
    void startReceiverThreads();

private:
    GMainLoop* mainLoop;
    GstRTSPServer* server;
    std::vector<ChannelContext*> channels;
    std::vector<std::thread> receiverThreads;
    bool running;
    OnStatsUpdate statsCb;
};

#endif // RTSP_PROXY_HPP
