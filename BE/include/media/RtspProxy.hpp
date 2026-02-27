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

// Forward declaration
struct ChannelContext;

struct ChannelStats {
    double fps;
    double bitrate_kbps;
    double proxy_latency_ms;

    // RTCP stats (새로 추가)
    uint64_t rtp_packets_received = 0;
    int32_t  rtp_packets_lost     = 0;     // docs에 int
    double   rtp_jitter_ms        = 0.0;
    uint64_t rtp_bytes_received   = 0;
    double rtp_round_trip_ms = 0.0;  // ← 새로 추가: RTT (ms)
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

    // rtcp 콜백
    static void onNewRtpManager(GstElement *rtspsrc, GstElement *manager, gpointer user_data);
    static void onSsrcActive(GstElement *rtpbin, guint session, guint ssrc, gpointer user_data);

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
