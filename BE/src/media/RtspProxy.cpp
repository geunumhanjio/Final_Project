#include "media/RtspProxy.hpp"
#include "media/GstQualityMonitor.hpp"
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <iostream>
#include <chrono>
#include <fstream>
#include <ctime>
#include <sstream>
#include <iomanip>

// ==================================================================================
// [Constants] CCTV URLs
// ==================================================================================
const std::string URL_CH1_MOB = "rtsp://admin:5hanwha!@192.168.0.16/0/MOBILE/media.smp";
const std::string URL_CH2_MOB = "rtsp://admin:5hanwha!@192.168.0.16/1/MOBILE/media.smp";
const std::string URL_CH3_MOB = "rtsp://admin:5hanwha!@192.168.0.16/2/MOBILE/media.smp";
const std::string URL_CH4_MOB = "rtsp://admin:5hanwha!@192.168.0.16/3/MOBILE/media.smp";

const std::string URL_CH1_FHD = "rtsp://admin:5hanwha!@192.168.0.16/0/H.264/media.smp";
const std::string URL_CH2_FHD = "rtsp://admin:5hanwha!@192.168.0.16/1/H.264/media.smp";
const std::string URL_CH3_FHD = "rtsp://admin:5hanwha!@192.168.0.16/2/H.264/media.smp";
const std::string URL_CH4_FHD = "rtsp://admin:5hanwha!@192.168.0.16/3/H.264/media.smp";

// Robot Cam (HTTP MJPEG)
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
        for (auto* client_appsrc : clients_copy) {
            GstBuffer *push_buffer = gst_buffer_copy(buffer);
            gst_app_src_push_buffer(GST_APP_SRC(client_appsrc), push_buffer);
            gst_object_unref(client_appsrc); // Release our local ref
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
            
            // 파이프라인 내부의 qualitymonitor 엘리먼트를 찾아서 통계 획득
            GstElement* qmon = gst_bin_get_by_name(GST_BIN(ctx->receiver_pipeline), "qmon");
            if (qmon) {
                GstQualityMonitor* monitor = GST_QUALITY_MONITOR(qmon);
                if (monitor->collector) {
                    stats.rtp = monitor->collector->getMetrics();
                }
                gst_object_unref(qmon);
            }

            // 콘솔 출력
            std::cout << "[" << ctx->path << "] RTCP Stats (via Element): "
                      << "pkts=" << stats.rtp.packets_received
                      << ", lost=" << stats.rtp.packets_lost
                      << ", jitter=" << std::fixed << std::setprecision(3) << stats.rtp.jitter_ms << " ms"
                      << ", rtt=" << stats.rtp.rtt_ms << " ms"
                      << std::endl;

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

RtspProxy::RtspProxy() : mainLoop(nullptr), server(nullptr), running(false) {
}

// channel context 소멸자가 있나?? 체크 필요
RtspProxy::~RtspProxy() {
    stop();
    for (auto* ctx : channels) {
        delete ctx;
    }
    channels.clear();
}

void RtspProxy::setupChannels() {
    // 1~4: Mobile
    channels.push_back(new ChannelContext(1, "/ch1", URL_CH1_MOB, this));
    channels.push_back(new ChannelContext(2, "/ch2", URL_CH2_MOB, this));
    channels.push_back(new ChannelContext(3, "/ch3", URL_CH3_MOB, this));
    channels.push_back(new ChannelContext(4, "/ch4", URL_CH4_MOB, this));

    // 5~8: FHD
    channels.push_back(new ChannelContext(5, "/ch1_fhd", URL_CH1_FHD, this));
    channels.push_back(new ChannelContext(6, "/ch2_fhd", URL_CH2_FHD, this));
    channels.push_back(new ChannelContext(7, "/ch3_fhd", URL_CH3_FHD, this));
    channels.push_back(new ChannelContext(8, "/ch4_fhd", URL_CH4_FHD, this));

    // 9: Robot Cam (MJPEG)
    //channels.push_back(new ChannelContext(9, "/robot_cam", URL_ROBOT_CAM, this));
}

bool RtspProxy::initialize(int port) {
    setupChannels();

    mainLoop = g_main_loop_new(NULL, FALSE);
    server = gst_rtsp_server_new();
    
    gchar* port_str = g_strdup_printf("%d", port);
    gst_rtsp_server_set_service(server, port_str);
    g_free(port_str);
    
    gst_rtsp_server_set_address(server, "0.0.0.0");

    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);

    for (auto* ctx : channels) {
        GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();
        gst_rtsp_media_factory_set_launch(factory, "( appsrc name=src ! rtph264pay name=pay0 pt=96 config-interval=-1 )");
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
        std::thread t(RtspProxy::runReceiverThread, ctx);
        t.detach(); 
        
        std::cout << "⏳ [System] Staggering start... waiting 2s" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void RtspProxy::start() {
    startReceiverThreads();
    running = true;
}

void RtspProxy::stop() {
    running = false;
    if (mainLoop && g_main_loop_is_running(mainLoop)) {
        g_main_loop_quit(mainLoop);
    }
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

void RtspProxy::mediaConfigure(GstRTSPMediaFactory *factory, GstRTSPMedia *media, gpointer user_data) {
    ChannelContext *ctx = (ChannelContext*)user_data;
    GstElement *element = gst_rtsp_media_get_element(media);
    GstElement *appsrc = gst_bin_get_by_name_recurse_up(GST_BIN(element), "src");

    if (appsrc) {
        g_object_set(G_OBJECT(appsrc), "format", GST_FORMAT_TIME, "do-timestamp", TRUE, "is-live", TRUE, NULL);
        
        std::lock_guard<std::mutex> lock(ctx->mutex);
        if (ctx->saved_caps) {
            g_object_set(G_OBJECT(appsrc), "caps", ctx->saved_caps, NULL);
        }
        ctx->clients.push_back(appsrc);
    }

    gst_object_unref(element);
}

void RtspProxy::runReceiverThread(ChannelContext *ctx) {
    while (true) {
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
                "latency=2000 drop-on-latency=false "
                "tcp-timeout=10000000 "     
                "do-rtcp=true "             
                "udp-buffer-size=33554432 ! "     
                "qualitymonitor name=qmon ! "
                "rtph264depay ! "
                "h264parse config-interval=-1 ! " 
                "appsink name=mysink emit-signals=true sync=false drop=true max-buffers=1",
                ctx->url.c_str());
        }

        ctx->receiver_pipeline = gst_parse_launch(pipeline_str, NULL);

        if (ctx->receiver_pipeline) {
            // Bus Watch
            GstBus *bus = gst_element_get_bus(ctx->receiver_pipeline);
            gst_bus_add_watch(bus, [](GstBus *bus, GstMessage *msg, gpointer data) -> gboolean {
                GMainLoop *loop = (GMainLoop *)data;
                switch (GST_MESSAGE_TYPE(msg)) {
                    case GST_MESSAGE_ERROR: {
                        GError *err;
                        gst_message_parse_error(msg, &err, NULL);
                        std::cerr << "❌ [" << ((ChannelContext*)g_main_loop_get_context(loop))->path 
                                  << "] Error: " << err->message << std::endl;
                        g_error_free(err);
                        g_main_loop_quit(loop);
                        break;
                    }
                    case GST_MESSAGE_EOS:
                        g_main_loop_quit(loop);
                        break;
                    default: break;
                }
                return TRUE;
            }, loop);
            gst_object_unref(bus);

            GstElement *appsink = gst_bin_get_by_name(GST_BIN(ctx->receiver_pipeline), "mysink");
            g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample), ctx);
            gst_object_unref(appsink);

            // Watchdog
            struct WatchdogData { ChannelContext *ctx; GMainLoop *loop; int checks; };
            WatchdogData *wd_data = new WatchdogData{ctx, loop, 0};

            g_timeout_add(1000, [](gpointer data) -> gboolean {
                WatchdogData *wd = (WatchdogData*)data;
                wd->checks++;
                
                bool has_caps = false;
                {
                    std::lock_guard<std::mutex> lock(wd->ctx->mutex);
                    if (wd->ctx->saved_caps) has_caps = true;
                }
                
                if (has_caps) { 
                    delete wd; return FALSE;
                }
                
                if (wd->checks >= 5) { 
                    std::cerr << "🚨 [" << wd->ctx->path << "] Caps timeout -> Reconnecting" << std::endl;
                    g_main_loop_quit(wd->loop);
                    delete wd; return FALSE;
                }
                return TRUE;
            }, wd_data);

            std::cout << "🔄 [" << ctx->path << "] Connecting..." << std::endl;
            gst_element_set_state(ctx->receiver_pipeline, GST_STATE_PLAYING);
            g_main_loop_run(loop);

            gst_element_set_state(ctx->receiver_pipeline, GST_STATE_NULL);
            gst_object_unref(ctx->receiver_pipeline);
        }

        g_main_loop_unref(loop);
        g_main_context_pop_thread_default(context);
        g_main_context_unref(context);

        std::cout << "⏳ [" << ctx->path << "] Retry in 2s..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}
