#include "media/RtspProxy.hpp"
#include "media/GstQualityMonitor.hpp"
#include "media/CctvScanner.hpp"
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <iostream>
#include <chrono>
#include <fstream>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

// ==================================================================================
// [Constants] CCTV Config
// ==================================================================================
const std::string TARGET_MAC = "E4:30:22:F2:D1:9B";
const std::string CCTV_CREDENTIALS = "admin:23wjdrms%40";

// Robot Cam (HTTP MJPEG) - This one is fixed or found via other means
const std::string URL_ROBOT_CAM = "http://192.168.0.237:8080/stream?topic=/camera/image_raw&type=mjpeg";

// ==================================================================================
// [Struct] ChannelContext Definition
// ==================================================================================
struct ChannelContext {
    int id;
    std::string path;
    std::string url;
    class RtspProxy* parent; // Pointer back to parent for callbacks
    
    GstElement *receiver_pipeline;
    std::vector<GstElement*> clients;
    std::mutex mutex;
    GstCaps *saved_caps;

    // Recording fields
    bool is_recording;
    bool need_keyframe;            // Wait for first keyframe
    GstClockTime record_start_pts; // Base timestamp for recording
    GstElement *record_pipeline;
    GstElement *record_appsrc;
    std::string current_filename;

    // Stats tracking
    uint64_t frameCount;
    uint64_t byteCount;
    double totalLatencyMs;
    std::chrono::steady_clock::time_point lastStatTime;

    // 통계 수집기 모듈
    GstStatsCollector statsCollector;

    ChannelContext(int i, std::string p, std::string u, RtspProxy* par) 
        : id(i), path(p), url(u), parent(par), receiver_pipeline(nullptr), saved_caps(nullptr),
          is_recording(false), need_keyframe(false), record_start_pts(GST_CLOCK_TIME_NONE),
          record_pipeline(nullptr), record_appsrc(nullptr),
          frameCount(0), byteCount(0), totalLatencyMs(0),
          lastStatTime(std::chrono::steady_clock::now()) {}

    ~ChannelContext() {
        std::lock_guard<std::mutex> lock(mutex);
        if (receiver_pipeline) {
            gst_element_set_state(receiver_pipeline, GST_STATE_NULL);
            gst_object_unref(receiver_pipeline);
        }
        if (saved_caps) gst_caps_unref(saved_caps);
        for (auto* client : clients) {
            gst_object_unref(client);
        }
        clients.clear();
        if (record_pipeline) {
            gst_element_set_state(record_pipeline, GST_STATE_NULL);
            gst_object_unref(record_pipeline);
        }
        if (record_appsrc) gst_object_unref(record_appsrc);
    }
};

// ==================================================================================
// [Helper] on_new_sample Callback (Optimized)
// ==================================================================================
static GstFlowReturn on_new_sample(GstElement *sink, gpointer user_data) {
    ChannelContext *ctx = (ChannelContext*)user_data;
    auto frame_start_time = std::chrono::high_resolution_clock::now();

    GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
    if (!sample) return GST_FLOW_ERROR;

    // Variables to hold copied state
    std::vector<GstElement*> clients_copy;
    bool is_recording_copy = false;
    GstElement* record_appsrc_copy = nullptr;

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    size_t buffer_size = buffer ? gst_buffer_get_size(buffer) : 0;

    // 1. Critical Section: Capture state and update counters
    {
        std::lock_guard<std::mutex> lock(ctx->mutex);

        // Update stats counters
        ctx->frameCount++;
        ctx->byteCount += buffer_size;

        // Capture Caps if needed
        if (!ctx->saved_caps) {
            GstCaps *caps = gst_sample_get_caps(sample);
            if (caps && !gst_caps_is_empty(caps) && !gst_caps_is_any(caps)) {
                ctx->saved_caps = gst_caps_copy(caps);
                gchar *str = gst_caps_to_string(ctx->saved_caps);
                std::cout << "\n✅ [" << ctx->path << "] Format captured -> " << str << "\n" << std::endl;
                g_free(str);
            }
        }

        // Copy clients list to local vector
        if (!ctx->clients.empty()) {
            clients_copy = ctx->clients;
        }

        // Copy recording state
        if (ctx->is_recording && ctx->record_appsrc) {
            is_recording_copy = true;
            record_appsrc_copy = ctx->record_appsrc;
            // Increase refcount because we use it outside mutex
            gst_object_ref(record_appsrc_copy);
        }
        
        // Also increase refcount for clients
        for (auto* client : clients_copy) {
            gst_object_ref(client);
        }
    }

    if (buffer) {
        // 2. Push to RTSP clients (Outside Mutex)
        std::vector<GstElement*> failed_clients;
        for (auto* client_appsrc : clients_copy) {
            GstBuffer *push_buffer = gst_buffer_copy(buffer);
            GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(client_appsrc), push_buffer);
            
            if (ret != GST_FLOW_OK && ret != GST_FLOW_FLUSHING) {
                failed_clients.push_back(client_appsrc);
            }
            gst_object_unref(client_appsrc); // Release our local ref
        }

        // Cleanup failed clients if any
        if (!failed_clients.empty()) {
            std::lock_guard<std::mutex> lock(ctx->mutex);
            for (auto* failed : failed_clients) {
                auto it = std::find(ctx->clients.begin(), ctx->clients.end(), failed);
                if (it != ctx->clients.end()) {
                    ctx->clients.erase(it);
                    gst_object_unref(failed); // Release original ref in vector
                    std::cout << "⚠️ [Cleanup] Removed non-responsive client from " << ctx->path << std::endl;
                }
            }
        }

        // 3. Push to Recorder (Outside Mutex)
        if (is_recording_copy && record_appsrc_copy) {
             // Check for keyframe (I-Frame)
             bool is_keyframe = !GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);

             if (ctx->need_keyframe) {
                 if (is_keyframe) {
                     std::cout << "⏺️ [Record] Keyframe found. Recording starts now." << std::endl;
                     ctx->need_keyframe = false;
                     ctx->record_start_pts = GST_BUFFER_PTS(buffer);
                 } else {
                     // Drop packets until first keyframe
                     gst_object_unref(record_appsrc_copy);
                     goto calc_latency; 
                 }
             }

             GstBuffer *push_buffer = gst_buffer_copy(buffer);

             // Re-timestamp relative to start
             if (GST_CLOCK_TIME_IS_VALID(ctx->record_start_pts)) {
                 if (GST_BUFFER_PTS(push_buffer) >= ctx->record_start_pts)
                     GST_BUFFER_PTS(push_buffer) -= ctx->record_start_pts;
                 
                 if (GST_BUFFER_DTS(push_buffer) >= ctx->record_start_pts)
                     GST_BUFFER_DTS(push_buffer) -= ctx->record_start_pts;
             }

             gst_app_src_push_buffer(GST_APP_SRC(record_appsrc_copy), push_buffer);
             gst_object_unref(record_appsrc_copy); // Release our local ref
        }
    } else {
        // If no buffer but we took refs, release them
        for (auto* client : clients_copy) gst_object_unref(client);
        if (record_appsrc_copy) gst_object_unref(record_appsrc_copy);
    }

calc_latency:
    // Calculate Proxy Latency for this frame
    auto frame_end_time = std::chrono::high_resolution_clock::now();
    double latency = std::chrono::duration<double, std::milli>(frame_end_time - frame_start_time).count();

    // 4. Update stats and trigger callback if interval passed
    {
        std::lock_guard<std::mutex> lock(ctx->mutex);
        ctx->totalLatencyMs += latency;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - ctx->lastStatTime).count();

        if (elapsed >= 1000) { // 1 second interval
            ChannelStats stats;
            stats.fps = (double)ctx->frameCount / (elapsed / 1000.0);
            stats.bitrate_kbps = ((double)ctx->byteCount * 8.0) / elapsed; 
            stats.proxy_latency_ms = ctx->totalLatencyMs / (double)ctx->frameCount;
            
            // [FIX] Ensure receiver_pipeline is valid before use
            if (ctx->receiver_pipeline) {
                GstElement* qmon = gst_bin_get_by_name(GST_BIN(ctx->receiver_pipeline), "qmon");
                if (qmon) {
                    GstQualityMonitor* monitor = GST_QUALITY_MONITOR(qmon);
                    if (monitor->collector) {
                        stats.rtp = monitor->collector->getMetrics();
                    }
                    gst_object_unref(qmon);
                }
            }

            /*
            std::cout << "[" << ctx->path << "] Stats: "
                      << "FPS=" << std::fixed << std::setprecision(1) << stats.fps
                      << ", Bitrate=" << (int)stats.bitrate_kbps << " kbps"
                      << ", Latency=" << std::fixed << std::setprecision(2) << stats.proxy_latency_ms << " ms"
                      << ", RTP(pkts=" << stats.rtp.packets_received
                      << ", lost=" << stats.rtp.packets_lost
                      << ", jitter=" << stats.rtp.jitter_ms << ")"
                      << std::endl;
            */
           
            if (ctx->parent) {
                ctx->parent->triggerStatsCallback(ctx->id, stats);
            }
            // Reset counters
            ctx->frameCount = 0;
            ctx->byteCount = 0;
            ctx->totalLatencyMs = 0;
            ctx->lastStatTime = now;
        }
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

// ==================================================================================
// [Class] RtspProxy Implementation
// ==================================================================================

RtspProxy::RtspProxy() : mainLoop(nullptr), server(nullptr), secure_server(nullptr), ssl_ctx(nullptr), rtsps_listen_fd(-1), running(false) {
}

// channel context 소멸자가 있나?? 체크 필요
RtspProxy::~RtspProxy() {
    stop();
    for (auto* ctx : channels) {
        delete ctx;
    }
    channels.clear();
    
    if (ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
    }

    if (secure_server) {
        g_object_unref(secure_server);
    }
}

bool RtspProxy::setupChannels() {
    std::string ip = "";
    const int max_retries = 10;
    const int retry_interval_sec = 3;

    for (int i = 1; i <= max_retries; ++i) {
        ip = CctvScanner::discoverIp(TARGET_MAC);
        if (!ip.empty()) {
            std::cout << "✅ [Proxy] CCTV discovered on attempt " << i << " at " << ip << std::endl;
            break;
        }
        
        std::cerr << "⚠️ [Proxy] CCTV discovery failed (Attempt " << i << "/" << max_retries << "). "
                  << "Retrying in " << retry_interval_sec << "s..." << std::endl;
        
        if (i < max_retries) {
            std::this_thread::sleep_for(std::chrono::seconds(retry_interval_sec));
        }
    }

    if (ip.empty()) {
        std::cerr << "❌ [Proxy] Critical Error: Failed to discover CCTV IP after " << max_retries << " attempts." << std::endl;
        return false;
    }

    std::cout << "🚀 [Proxy] Setting up channels for CCTV at " << ip << std::endl;

    // Base URL template: rtsp://ID:PW@IP/channel/profile/media.smp
    auto make_url = [&](int channel, bool fhd) {
        std::string profile = fhd ? "H.264" : "MOBILE";
        return "rtsp://" + CCTV_CREDENTIALS + "@" + ip + "/" + std::to_string(channel) + "/" + profile + "/media.smp";
    };

    // 1~4: Mobile
    channels.push_back(new ChannelContext(1, "/ch1", make_url(0, false), this));
    channels.push_back(new ChannelContext(2, "/ch2", make_url(1, false), this));
    channels.push_back(new ChannelContext(3, "/ch3", make_url(2, false), this));
    channels.push_back(new ChannelContext(4, "/ch4", make_url(3, false), this));

    // 5~8: FHD
    channels.push_back(new ChannelContext(5, "/ch1_fhd", make_url(0, true), this));
    channels.push_back(new ChannelContext(6, "/ch2_fhd", make_url(1, true), this));
    channels.push_back(new ChannelContext(7, "/ch3_fhd", make_url(2, true), this));
    channels.push_back(new ChannelContext(8, "/ch4_fhd", make_url(3, true), this));

    return true;
}

bool RtspProxy::initSSLContext() {
    // Legacy SSL Context Initialization (Keep for manual testing if needed)
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx) return false;

    // TLS 1.2 미만 차단
    SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_2_VERSION);

    if (SSL_CTX_use_certificate_file(ssl_ctx, "/etc/rtsps/certs/server.crt", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, "/etc/rtsps/certs/server.key", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    if (!SSL_CTX_check_private_key(ssl_ctx)) {
        fprintf(stderr, "[SSL] cert/key mismatch\n");
        return false;
    }

    printf("[SSL] Context initialized OK\n");
    return true;
}

bool RtspProxy::startRTSPS(int port) {
    std::cout << "🛡️ [RTSPS] Starting Native Secure RTSP Server on port " << port << "..." << std::endl;
    
    // GStreamer Native TLS Implementation
    secure_server = gst_rtsp_server_new();
    
    gchar* port_str = g_strdup_printf("%d", port);
    gst_rtsp_server_set_service(secure_server, port_str);
    g_free(port_str);
    
    gst_rtsp_server_set_address(secure_server, "0.0.0.0");

    // Load TLS Certificate for GStreamer
    GError *error = NULL;
    GTlsCertificate *cert = g_tls_certificate_new_from_files(
        "/etc/rtsps/certs/server.crt", 
        "/etc/rtsps/certs/server.key", 
        &error
    );

    if (error) {
        std::cerr << "❌ [RTSPS] Failed to load TLS certificate: " << error->message << std::endl;
        g_error_free(error);
        return false;
    }

    // Modern GStreamer 1.18+ TLS Setup via GstRTSPAuth
    GstRTSPAuth *auth = gst_rtsp_auth_new();
    gst_rtsp_auth_set_tls_certificate(auth, cert);
    g_object_unref(cert);

    // [Anonymous Access Setup]
    // Create a default token with 'anonymous' role
    GstRTSPToken *token = gst_rtsp_token_new(GST_RTSP_TOKEN_MEDIA_FACTORY_ROLE, G_TYPE_STRING, "anonymous", NULL);
    gst_rtsp_auth_set_default_token(auth, token);
    gst_rtsp_token_unref(token);

    // Apply Auth (including TLS) to the server
    gst_rtsp_server_set_auth(secure_server, auth);
    g_object_unref(auth);

    // [FIX] Add connection log for TLS Handshake phase
    g_signal_connect(secure_server, "client-connected", G_CALLBACK(+[](GstRTSPServer* server, GstRTSPClient* client, gpointer user_data) {
        GstRTSPConnection* conn = gst_rtsp_client_get_connection(client);
        const gchar* ip = gst_rtsp_connection_get_ip(conn);
        std::cout << "🛡️ [RTSPS] New secure connection attempt from: " << (ip ? ip : "unknown") << std::endl;
    }), NULL);

    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(secure_server);

    for (auto* ctx : channels) {
        GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();
        // [FIX] Add queue for buffering and set config-interval=1 for frequent header sync
        gst_rtsp_media_factory_set_launch(factory, "( appsrc name=src ! queue max-size-buffers=30 ! rtph264pay name=pay0 pt=96 config-interval=1 )");
        gst_rtsp_media_factory_set_shared(factory, FALSE);
        
        // Explicitly allow 'anonymous' role to access and construct this media without password
        gst_rtsp_media_factory_add_role(factory, "anonymous",
            GST_RTSP_PERM_MEDIA_FACTORY_ACCESS, G_TYPE_BOOLEAN, TRUE,
            GST_RTSP_PERM_MEDIA_FACTORY_CONSTRUCT, G_TYPE_BOOLEAN, TRUE,
            NULL);
        
        // Connect signal to static member
        g_signal_connect(factory, "media-configure", G_CALLBACK(RtspProxy::mediaConfigure), ctx);
        
        gst_rtsp_mount_points_add_factory(mounts, ctx->path.c_str(), factory);
        
        std::cout << "✅ [RTSPS] Registered secure path: rtsps://IP:" << port << ctx->path << std::endl;
    }
    g_object_unref(mounts);

    if (gst_rtsp_server_attach(secure_server, NULL) == 0) {
        std::cerr << "❌ [RTSPS] Failed to bind port " << port << "!" << std::endl;
        return false;
    }

    std::cout << "✅ [RTSPS] Native Secure RTSP Server is operational (Anonymous Allowed)." << std::endl;

    return true;
}

void RtspProxy::runRTSPSLoop(int port) {
    rtsps_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(rtsps_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(rtsps_listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind RTSPS");
        return;
    }
    
    listen(rtsps_listen_fd, 64);
    printf("[RTSPS] Listening on port %d\n", port);
    fflush(stdout);

    while (running) {
        struct sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(rtsps_listen_fd, (struct sockaddr*)&client_addr, &len);
        if (client_fd < 0) {
            if (running) perror("accept RTSPS");
            continue;
        }

        SSL* ssl = SSL_new(ssl_ctx);
        SSL_set_fd(ssl, client_fd);

        if (SSL_accept(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            close(client_fd);
            continue;
        }

        printf("[RTSPS] Secure connection accepted from %s, cipher: %s\n", 
               inet_ntoa(client_addr.sin_addr), SSL_get_cipher(ssl));
        fflush(stdout);

        SecureClient* sclient = new SecureClient{client_fd, ssl};
        std::thread clientThread(&RtspProxy::handleSecureClient, this, sclient);
        clientThread.detach();
    }
}

void RtspProxy::handleSecureClient(SecureClient* client) {
    char buffer[4096];
    
    while (running) {
        int bytes = SSL_read(client->ssl, buffer, sizeof(buffer) - 1);
        if (bytes <= 0) {
            int err = SSL_get_error(client->ssl, bytes);
            if (err != SSL_ERROR_ZERO_RETURN) {
                fprintf(stderr, "[RTSPS] SSL_read error: %d\n", err);
            }
            break;
        }
        buffer[bytes] = '\0';
        printf("[RTSPS] Received (%d bytes): %s\n", bytes, buffer);
        fflush(stdout);

        // 간단한 RTSP OPTIONS 응답 예시 (테스트용)
        if (strstr(buffer, "OPTIONS")) {
            const char* response = 
                "RTSP/1.0 200 OK\r\n"
                "CSeq: 1\r\n"
                "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, OPTIONS\r\n"
                "\r\n";
            SSL_write(client->ssl, response, strlen(response));
        }
    }

    printf("[RTSPS] Closing secure connection\n");
    fflush(stdout);
    SSL_shutdown(client->ssl);
    SSL_free(client->ssl);
    close(client->fd);
    delete client;
}

bool RtspProxy::initialize(int port) {
    if (!setupChannels()) {
        return false;
    }

    mainLoop = g_main_loop_new(NULL, FALSE);
    server = gst_rtsp_server_new();
    
    gchar* port_str = g_strdup_printf("%d", port);
    gst_rtsp_server_set_service(server, port_str);
    g_free(port_str);
    
    gst_rtsp_server_set_address(server, "0.0.0.0");

    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);

    for (auto* ctx : channels) {
        GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();
        // [FIX] Add queue for buffering and set config-interval=1 for frequent header sync
        gst_rtsp_media_factory_set_launch(factory, "( appsrc name=src ! queue max-size-buffers=30 ! rtph264pay name=pay0 pt=96 config-interval=1 )");
        gst_rtsp_media_factory_set_shared(factory, FALSE);
        
        // Connect signal to static member
        g_signal_connect(factory, "media-configure", G_CALLBACK(RtspProxy::mediaConfigure), ctx);
        
        gst_rtsp_mount_points_add_factory(mounts, ctx->path.c_str(), factory);
        
        std::cout << "✅ Registered path: rtsp://IP:" << port << ctx->path << std::endl;
    }
    g_object_unref(mounts);

    if (gst_rtsp_server_attach(server, NULL) == 0) {
        std::cerr << "❌ Failed to bind port " << port << "!" << std::endl;
        return false;
    }

    return true;
}

void RtspProxy::startReceiverThreads() {
    for (auto* ctx : channels) {
        receiverThreads.emplace_back(RtspProxy::runReceiverThread, ctx);
        
        std::cout << "⏳ [System] Staggering start (" << ctx->path << ")... waiting 2s" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void RtspProxy::start() {
    running = true;
    startReceiverThreads();
}

void RtspProxy::stop() {
    if (!running) return;
    running = false;

    std::cout << "⏹️ [Proxy] Stopping all services..." << std::endl;

    if (mainLoop && g_main_loop_is_running(mainLoop)) {
        g_main_loop_quit(mainLoop);
    }

    // Join receiver threads
    for (auto& t : receiverThreads) {
        if (t.joinable()) {
            t.join();
        }
    }
    receiverThreads.clear();
}

void RtspProxy::run() {
    if (mainLoop) {
        g_main_loop_run(mainLoop);
    }
}

bool RtspProxy::startRecording(int channelId) {
    ChannelContext* target = nullptr;
    for (auto* ctx : channels) {
        if (ctx->id == channelId) {
            target = ctx;
            break;
        }
    }

    if (!target) {
        std::cerr << "❌ [Record] Invalid channel ID: " << channelId << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(target->mutex);
    if (target->is_recording) {
        std::cerr << "⚠️ [Record] Channel " << channelId << " is already recording!" << std::endl;
        return false;
    }

    if (!target->saved_caps) {
        std::cerr << "❌ [Record] No stream caps available yet. Cannot start recording." << std::endl;
        return false;
    }

    // Generate filename: rec_chX_YYYYMMDD_HHMMSS.mp4
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "rec_ch" << channelId << "_" << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S") << ".mp4";
    target->current_filename = ss.str();

    // Reset recording state
    target->need_keyframe = true;
    target->record_start_pts = GST_CLOCK_TIME_NONE;

    // Create recording pipeline
    // appsrc -> h264parse -> mp4mux -> filesink
    // Note: removed do-timestamp=true to use original PTS
    std::string pipelineStr = "appsrc name=rec_src format=time is-live=true ! h264parse ! mp4mux ! filesink location="+ target->current_filename;
    
    GError *err = nullptr;
    target->record_pipeline = gst_parse_launch(pipelineStr.c_str(), &err);

    if (!target->record_pipeline || err) {
        std::cerr << "❌ [Record] Failed to create pipeline: " << (err ? err->message : "Unknown") << std::endl;
        if (err) g_error_free(err);
        return false;
    }

    target->record_appsrc = gst_bin_get_by_name(GST_BIN(target->record_pipeline), "rec_src");
    
    // Set caps on appsrc
    g_object_set(G_OBJECT(target->record_appsrc), "caps", target->saved_caps, NULL);

    gst_element_set_state(target->record_pipeline, GST_STATE_PLAYING);
    target->is_recording = true;

    std::cout << "🔴 [Record] Started on Ch " << channelId << " -> " << target->current_filename << std::endl;
    return true;
}

std::string RtspProxy::stopRecording(int channelId) {
    ChannelContext* target = nullptr;
    for (auto* ctx : channels) {
        if (ctx->id == channelId) {
            target = ctx;
            break;
        }
    }

    if (!target) return "";

    std::string filename;
    {
        std::lock_guard<std::mutex> lock(target->mutex);
        if (!target->is_recording) return "";

        std::cout << "⏹️ [Record] Stopping on Ch " << channelId << "..." << std::endl;
        
        bool was_waiting_for_keyframe = target->need_keyframe;
        target->is_recording = false;
        
        // Send EOS to finish file properly
        if (target->record_appsrc) {
            gst_app_src_end_of_stream(GST_APP_SRC(target->record_appsrc));
        }

        // Wait for EOS on bus (simple blocking wait for prototype)
        GstBus *bus = gst_element_get_bus(target->record_pipeline);
        GstMessage *msg = gst_bus_timed_pop_filtered(bus, 3 * GST_SECOND, (GstMessageType)(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
        
        if (msg) {
            if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
                GError *err = nullptr;
                gchar *debug = nullptr;
                gst_message_parse_error(msg, &err, &debug);
                std::cerr << "❌ [Record] Pipeline error during stop: " << (err ? err->message : "Unknown") << std::endl;
                if (err) g_error_free(err);
                g_free(debug);
            } else {
                std::cout << "✅ [Record] EOS received successfully." << std::endl;
            }
            gst_message_unref(msg);
        } else {
            std::cerr << "⚠️ [Record] EOS timeout! File might be corrupted or empty." << std::endl;
        }
        gst_object_unref(bus);

        gst_element_set_state(target->record_pipeline, GST_STATE_NULL);
        
        if (target->record_appsrc) gst_object_unref(target->record_appsrc);
        gst_object_unref(target->record_pipeline);

        target->record_pipeline = nullptr;
        target->record_appsrc = nullptr;

        // Final check: file existence and size
        std::ifstream tmpFile(target->current_filename, std::ios::binary | std::ios::ate);
        std::streamsize actualSize = tmpFile.is_open() ? tmpFile.tellg() : (std::basic_istream<char>::pos_type)0;
        tmpFile.close();

        if (was_waiting_for_keyframe || actualSize <= 0) {
            if (actualSize <= 0) {
                std::cerr << "⚠️ [Record] File is 0 bytes. Deleting: " << target->current_filename << std::endl;
            } else {
                std::cerr << "⚠️ [Record] Stopped before first keyframe. Deleting: " << target->current_filename << std::endl;
            }
            remove(target->current_filename.c_str());
            filename = ""; 
        } else {
            filename = target->current_filename;
        }
    }

    if (!filename.empty()) {
        std::cout << "💾 [Record] Saved: " << filename << std::endl;
    }
    return filename;
}

// [Helper] 클라이언트 종료 시 호출되는 콜백
static void on_media_unprepared(GstRTSPMedia *media, gpointer user_data) {
    auto* pair = (std::pair<ChannelContext*, GstElement*>*)user_data;
    ChannelContext* ctx = pair->first;
    GstElement* appsrc = pair->second;

    std::lock_guard<std::mutex> lock(ctx->mutex);
    auto it = std::find(ctx->clients.begin(), ctx->clients.end(), appsrc);
    if (it != ctx->clients.end()) {
        ctx->clients.erase(it);
        std::cout << "🧹 [Cleanup] Client disconnected from " << ctx->path 
                  << ". Remaining: " << ctx->clients.size() << std::endl;
    }
    
    gst_object_unref(appsrc);
    delete pair;
}

void RtspProxy::mediaConfigure(GstRTSPMediaFactory *factory, GstRTSPMedia *media, gpointer user_data) {
    ChannelContext *ctx = (ChannelContext*)user_data;
    std::cout << "🎯 [RTSP/S] New client connecting to path: " << ctx->path << std::endl;
    
    GstElement *element = gst_rtsp_media_get_element(media);
    GstElement *appsrc = gst_bin_get_by_name_recurse_up(GST_BIN(element), "src");

    if (appsrc) {
        // [FIX] Ensure no internal byte-limit drops by setting max-bytes=0
        g_object_set(G_OBJECT(appsrc), "format", GST_FORMAT_TIME, "do-timestamp", TRUE, "is-live", TRUE, "max-bytes", 0, NULL);
        
        std::lock_guard<std::mutex> lock(ctx->mutex);
        if (ctx->saved_caps) {
            g_object_set(G_OBJECT(appsrc), "caps", ctx->saved_caps, NULL);
        }
        
        // Increase ref for our storage
        gst_object_ref(appsrc);
        ctx->clients.push_back(appsrc);

        // 클라이언트 종료 시 정리를 위해 시그널 연결
        auto* cleanup_data = new std::pair<ChannelContext*, GstElement*>(ctx, (GstElement*)gst_object_ref(appsrc));
        g_signal_connect(media, "unprepared", G_CALLBACK(on_media_unprepared), cleanup_data);
    }

    gst_object_unref(element);
}

void RtspProxy::runReceiverThread(ChannelContext *ctx) {
    while (ctx->parent->running) {
        GMainContext *context = g_main_context_new();
        g_main_context_push_thread_default(context);
        GMainLoop *loop = g_main_loop_new(context, FALSE);

        // Reset Caps
        {
            std::lock_guard<std::mutex> lock(ctx->mutex);
            if (ctx->saved_caps) {
                gst_caps_unref(ctx->saved_caps);
                ctx->saved_caps = nullptr;
            }
        }

        // Pipeline Construction
        char pipeline_str[2048];

        if (ctx->url.find("http://") == 0) {
            // MJPEG over HTTP (Transcoding to H.264)
            // souphttpsrc -> multipartdemux -> jpegdec -> videoconvert -> x264enc -> h264parse -> appsink
            sprintf(pipeline_str,
                "souphttpsrc location=%s ! "
                "multipartdemux ! "
                "jpegdec ! "
                "videoconvert ! "
                "video/x-raw,format=I420 !"
                "videoscale ! video/x-raw,width=640,height=480 !"
                "v4l2h264enc ! "
                "h264parse config-interval=1 ! "
                "appsink name=mysink emit-signals=true sync=false drop=true max-buffers=1",
                ctx->url.c_str());
        } else {
            // RTSP H.264 (Passthrough)
            sprintf(pipeline_str,
                "rtspsrc name=src location=%s protocols=tcp "
                "latency=1000 drop-on-latency=false "
                "tcp-timeout=10000000 "     
                "do-rtcp=true "             
                "ntp-sync=true ntp-time-source=running-time rtcp-sync-send=true "
                "udp-buffer-size=33554432 ! "     
                "qualitymonitor name=qmon ! "
                "rtph264depay ! "
                "h264parse config-interval=-1 ! " 
                "appsink name=mysink emit-signals=true sync=false drop=true max-buffers=1",
                ctx->url.c_str());
        }

        ctx->receiver_pipeline = gst_parse_launch(pipeline_str, NULL);

        if (ctx->receiver_pipeline) {
            // [FIX] Bus Watch with correct data
            GstBus *bus = gst_element_get_bus(ctx->receiver_pipeline);
            struct PipelineData { 
                ChannelContext *ctx; 
                GMainLoop *loop; 
            };
            PipelineData *p_data = new PipelineData{ctx, loop};

            gst_bus_add_watch(bus, [](GstBus *bus, GstMessage *msg, gpointer data) -> gboolean {
                PipelineData *pd = (PipelineData *)data;
                switch (GST_MESSAGE_TYPE(msg)) {
                    case GST_MESSAGE_ERROR: {
                        GError *err;
                        gst_message_parse_error(msg, &err, NULL);
                        std::cerr << "❌ [" << pd->ctx->path << "] Error: " << err->message << std::endl;
                        g_error_free(err);
                        g_main_loop_quit(pd->loop);
                        break;
                    }
                    case GST_MESSAGE_EOS:
                        std::cout << "ℹ️ [" << pd->ctx->path << "] End of Stream (EOS) received. Reconnecting..." << std::endl;
                        g_main_loop_quit(pd->loop);
                        break;
                    default: break;
                }
                return TRUE;
            }, p_data);
            gst_object_unref(bus);

            GstElement *appsink = gst_bin_get_by_name(GST_BIN(ctx->receiver_pipeline), "mysink");
            g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample), ctx);
            gst_object_unref(appsink);

            // [FIX] Watchdog using thread-local context to avoid dangling pointers
            GSource *timeout_source = g_timeout_source_new(1000);
            
            struct WatchdogData { 
                ChannelContext *ctx; 
                GMainLoop *loop; 
                int checks; 
            };
            WatchdogData *wd_data = new WatchdogData{ctx, loop, 0};

            g_source_set_callback(timeout_source, [](gpointer data) -> gboolean {
                WatchdogData *wd = (WatchdogData*)data;
                wd->checks++;
                
                bool has_caps = false;
                {
                    std::lock_guard<std::mutex> lock(wd->ctx->mutex);
                    if (wd->ctx->saved_caps) has_caps = true;
                }
                
                if (has_caps) { 
                    return FALSE; // Source will be removed
                }
                
                if (wd->checks >= 15) { 
                    std::cerr << "🚨 [" << wd->ctx->path << "] Caps timeout -> Reconnecting" << std::endl;
                    g_main_loop_quit(wd->loop);
                    return FALSE; // Source will be removed
                }
                return TRUE;
            }, wd_data, [](gpointer data){ 
                delete (WatchdogData*)data; 
            });

            g_source_attach(timeout_source, context);
            g_source_unref(timeout_source);

            std::cout << "🔄 [" << ctx->path << "] Connecting..." << std::endl;
            gst_element_set_state(ctx->receiver_pipeline, GST_STATE_PLAYING);
            g_main_loop_run(loop);

            // [FIX] Cleanup p_data and reset pipeline pointer under lock
            {
                std::lock_guard<std::mutex> lock(ctx->mutex);
                if (ctx->receiver_pipeline) {
                    gst_element_set_state(ctx->receiver_pipeline, GST_STATE_NULL);
                    gst_object_unref(ctx->receiver_pipeline);
                    ctx->receiver_pipeline = nullptr;
                }
            }
            delete p_data;
        }

        g_main_loop_unref(loop);
        g_main_context_pop_thread_default(context);
        g_main_context_unref(context);

        std::cout << "⏳ [" << ctx->path << "] Retry in 2s..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}
