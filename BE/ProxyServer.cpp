#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <iostream>
#include <vector>
#include <string>
#include <mutex>
#include <algorithm>
#include <thread>

// ==================================================================================
// [설정] CCTV 4대 주소 (Mobile & FHD)
// ==================================================================================
// 1. Mobile (기존)
const std::string URL_CH1_MOB = "rtsp://admin:5hanwha!@192.168.0.16/0/MOBILE/media.smp";
const std::string URL_CH2_MOB = "rtsp://admin:5hanwha!@192.168.0.16/1/MOBILE/media.smp";
const std::string URL_CH3_MOB = "rtsp://admin:5hanwha!@192.168.0.16/2/MOBILE/media.smp";
const std::string URL_CH4_MOB = "rtsp://admin:5hanwha!@192.168.0.16/3/MOBILE/media.smp";

// 2. FHD (신규 - MOBILE을 H.264로 변경)
const std::string URL_CH1_FHD = "rtsp://admin:5hanwha!@192.168.0.16/0/H.264/media.smp";
const std::string URL_CH2_FHD = "rtsp://admin:5hanwha!@192.168.0.16/1/H.264/media.smp";
const std::string URL_CH3_FHD = "rtsp://admin:5hanwha!@192.168.0.16/2/H.264/media.smp";
const std::string URL_CH4_FHD = "rtsp://admin:5hanwha!@192.168.0.16/3/H.264/media.smp";

// ==================================================================================
// [구조체] 채널별 독립적인 컨텍스트
// ==================================================================================
struct ChannelContext {
    int id;
    std::string path;   // 예: "/ch1", "/ch1_fhd"
    std::string url;    // RTSP 주소
    
    GstElement *receiver_pipeline;
    std::vector<GstElement*> clients;
    std::mutex mutex;
    GstCaps *saved_caps;

    ChannelContext(int i, std::string p, std::string u) 
        : id(i), path(p), url(u), receiver_pipeline(nullptr), saved_caps(nullptr) {}
};

// ==================================================================================
// [Receiver] 데이터 수신 및 배분
// ==================================================================================
static GstFlowReturn on_new_sample(GstElement *sink, gpointer user_data) {
    ChannelContext *ctx = (ChannelContext*)user_data;

    GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
    if (!sample) return GST_FLOW_ERROR;

    std::lock_guard<std::mutex> lock(ctx->mutex);

    // Caps 확보
    if (!ctx->saved_caps) {
        GstCaps *caps = gst_sample_get_caps(sample);
        if (caps && !gst_caps_is_empty(caps) && !gst_caps_is_any(caps)) {
            ctx->saved_caps = gst_caps_copy(caps);
            gchar *str = gst_caps_to_string(ctx->saved_caps);
            std::cout << "\n✅ [" << ctx->path << "] 포맷 확보 완료! -> " << str << "\n" << std::endl;
            g_free(str);
        }
    }

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    if (buffer) {
        // 클라이언트에게 전송
        auto it = ctx->clients.begin();
        while (it != ctx->clients.end()) {
            GstElement *client_appsrc = *it;
            GstBuffer *push_buffer = gst_buffer_copy(buffer);
            GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(client_appsrc), push_buffer);

            if (ret != GST_FLOW_OK) {
                it = ctx->clients.erase(it);
            } else {
                ++it;
            }
        }
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

// ==================================================================================
// [Receiver Thread] 무한 재접속 + Watchdog 탑재
// ==================================================================================
void run_receiver_thread(ChannelContext *ctx) {
    while (true) {
        GMainContext *context = g_main_context_new();
        g_main_context_push_thread_default(context);
        GMainLoop *loop = g_main_loop_new(context, FALSE);

        // Caps 초기화
        {
            std::lock_guard<std::mutex> lock(ctx->mutex);
            if (ctx->saved_caps) {
                gst_caps_unref(ctx->saved_caps);
                ctx->saved_caps = nullptr;
            }
        }

        // 파이프라인 생성 (FHD 데이터량 고려하여 버퍼 충분히 확보)
        char pipeline_str[2048];
        sprintf(pipeline_str,
            "rtspsrc location=%s protocols=tcp "
            "latency=2000 drop-on-latency=false "
            "tcp-timeout=10000000 "     
            "do-rtcp=true "             
            "udp-buffer-size=33554432 ! "     // [수정] FHD 고려하여 20MB -> 32MB 증설
            "rtph264depay ! "
            "h264parse config-interval=-1 ! " 
            "appsink name=mysink emit-signals=true sync=false drop=true max-buffers=1",
            ctx->url.c_str());

        ctx->receiver_pipeline = gst_parse_launch(pipeline_str, NULL);

        if (ctx->receiver_pipeline) {
            // 버스 감시 (에러/EOS 시 재접속)
            GstBus *bus = gst_element_get_bus(ctx->receiver_pipeline);
            gst_bus_add_watch(bus, [](GstBus *bus, GstMessage *msg, gpointer data) -> gboolean {
                GMainLoop *loop = (GMainLoop *)data;
                switch (GST_MESSAGE_TYPE(msg)) {
                    case GST_MESSAGE_ERROR: {
                        GError *err;
                        gst_message_parse_error(msg, &err, NULL);
                        std::cerr << "❌ [" << ((ChannelContext*)g_main_loop_get_context(loop))->path 
                                  << "] 에러: " << err->message << std::endl;
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

            // [Watchdog] 5초 내 포맷 미확보 시 강제 재접속
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
                
                if (wd->checks >= 5) { // 5초 타임아웃
                    std::cerr << "🚨 [" << wd->ctx->path << "] Caps 확보 실패 -> 강제 재접속" << std::endl;
                    g_main_loop_quit(wd->loop);
                    delete wd; return FALSE;
                }
                return TRUE;
            }, wd_data);

            std::cout << "🔄 [" << ctx->path << "] 연결 시도..." << std::endl;
            gst_element_set_state(ctx->receiver_pipeline, GST_STATE_PLAYING);
            g_main_loop_run(loop);

            gst_element_set_state(ctx->receiver_pipeline, GST_STATE_NULL);
            gst_object_unref(ctx->receiver_pipeline);
        }

        g_main_loop_unref(loop);
        g_main_context_pop_thread_default(context);
        g_main_context_unref(context);

        std::cout << "⏳ [" << ctx->path << "] 2초 후 재접속..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

// ==================================================================================
// [Sender] 설정
// ==================================================================================
void media_configure(GstRTSPMediaFactory *factory, GstRTSPMedia *media, gpointer user_data) {
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

// ==================================================================================
// 메인
// ==================================================================================
int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);

    std::vector<ChannelContext*> channels;

    // 1~4번: Mobile
    channels.push_back(new ChannelContext(1, "/ch1", URL_CH1_MOB));
    channels.push_back(new ChannelContext(2, "/ch2", URL_CH2_MOB));
    channels.push_back(new ChannelContext(3, "/ch3", URL_CH3_MOB));
    channels.push_back(new ChannelContext(4, "/ch4", URL_CH4_MOB));

    // 5~8번: FHD (추가됨)
    channels.push_back(new ChannelContext(5, "/ch1_fhd", URL_CH1_FHD));
    channels.push_back(new ChannelContext(6, "/ch2_fhd", URL_CH2_FHD));
    channels.push_back(new ChannelContext(7, "/ch3_fhd", URL_CH3_FHD));
    channels.push_back(new ChannelContext(8, "/ch4_fhd", URL_CH4_FHD));

    // Receiver 스레드 시작 (순차 실행)
    for (auto* ctx : channels) {
        std::thread t(run_receiver_thread, ctx);
        t.detach();
        std::cout << "⏳ [System] 안정적인 시작을 위해 2초 대기..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    // RTSP 서버 설정
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    GstRTSPServer *server = gst_rtsp_server_new();
    gst_rtsp_server_set_service(server, "8554");
    gst_rtsp_server_set_address(server, "0.0.0.0");

    GstRTSPMountPoints *mounts = gst_rtsp_server_get_mount_points(server);

    for (auto* ctx : channels) {
        GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();
        gst_rtsp_media_factory_set_launch(factory, "( appsrc name=src ! rtph264pay name=pay0 pt=96 config-interval=-1 )");
        gst_rtsp_media_factory_set_shared(factory, FALSE);
        g_signal_connect(factory, "media-configure", G_CALLBACK(media_configure), ctx);
        gst_rtsp_mount_points_add_factory(mounts, ctx->path.c_str(), factory);
        
        std::cout << "✅ 경로 등록: rtsp://IP:8554" << ctx->path << std::endl;
    }
    
    g_object_unref(mounts);

    if (gst_rtsp_server_attach(server, NULL) == 0) {
        std::cerr << "❌ 포트 8554 바인딩 실패!" << std::endl;
        return -1;
    }

    std::cout << "=================================================" << std::endl;
    std::cout << " 🚀 8-Channel (4 Mob + 4 FHD) Proxy Server" << std::endl;
    std::cout << "=================================================" << std::endl;

    g_main_loop_run(loop);
    return 0;
}