#ifndef GST_STATS_COLLECTOR_HPP
#define GST_STATS_COLLECTOR_HPP

#include <gst/gst.h>
#include <mutex>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>

struct RtpQualityMetrics {
    uint64_t packets_received = 0;
    int32_t  packets_lost = 0;
    double   jitter_ms = 0.0;
    uint64_t bytes_received = 0;
    double   rtt_ms = 0.0;
};

class GstStatsCollector {
public:
    GstStatsCollector() {}
    ~GstStatsCollector() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto* session : rtp_sessions_) {
            g_object_unref(session);
        }
        rtp_sessions_.clear();
    }

    // 파이프라인 내에서 rtpbin을 찾아 시그널을 연결합니다.
    void attach(GstElement* pipeline) {
        GstElement* rtpbin = gst_bin_get_by_name(GST_BIN(pipeline), "rtpbin");
        
        if (!rtpbin) {
            GstElement* src = gst_bin_get_by_name(GST_BIN(pipeline), "src");
            if (src) {
                g_signal_connect(src, "new-manager", G_CALLBACK(on_new_manager), this);
                gst_object_unref(src);
            }
        } else {
            bind_rtpbin(rtpbin);
            gst_object_unref(rtpbin);
        }
    }

    RtpQualityMetrics getMetrics() {
        std::lock_guard<std::mutex> lock(mutex_);
        RtpQualityMetrics metrics;
        
        // 감시 중인 모든 세션을 순회하며 가장 유효한 통계를 찾음
        for (auto* rtp_session : rtp_sessions_) {
            GstStructure *stats_struct = nullptr;
            g_object_get(rtp_session, "stats", &stats_struct, nullptr);

            if (stats_struct) {
                const GValue *val = gst_structure_get_value(stats_struct, "source-stats");
                if (val && G_VALUE_HOLDS(val, G_TYPE_VALUE_ARRAY)) {
                    GValueArray *arr = (GValueArray *)g_value_get_boxed(val);

                    for (guint i = 0; i < arr->n_values; ++i) {
                        const GValue *sval = g_value_array_get_nth(arr, i);
                        GstStructure *src_stats = (GstStructure *)g_value_get_boxed(sval);
                        if (!src_stats) continue;

                        gboolean is_sender = FALSE;
                        gst_structure_get_boolean(src_stats, "is-sender", &is_sender);

                        // 실제 데이터를 쏘고 있는 송신자(카메라)의 통계 수집
                        if (is_sender) {
                            uint64_t pkts = 0;
                            guint jitter_units = 0;
                            gint clock_rate = 90000;
                            guint rb_round_trip = 0;

                            gst_structure_get_uint64(src_stats, "packets-received", &pkts);
                            if (pkts == 0) continue; 

                            metrics.packets_received = pkts;
                            gst_structure_get_int(src_stats, "packets-lost", &metrics.packets_lost);
                            gst_structure_get_uint64(src_stats, "bytes-received", &metrics.bytes_received);
                            gst_structure_get_uint(src_stats, "jitter", &jitter_units);
                            gst_structure_get_int(src_stats, "clock-rate", &clock_rate);
                            metrics.jitter_ms = (clock_rate > 0) ? (jitter_units * 1000.0 / clock_rate) : 0.0;

                            // RTT 추출
                            if (gst_structure_get_uint(src_stats, "rb-round-trip", &rb_round_trip) && rb_round_trip > 0) {
                                double rtt_seconds = (rb_round_trip >> 16) + static_cast<double>(rb_round_trip & 0xFFFF) / 65536.0;
                                metrics.rtt_ms = rtt_seconds * 1000.0;
                            }
                            
                            // 만약 RTT가 여전히 0이라면, 혹시 rb-round-trip 대신 다른 필드에 정보가 있는지 체크
                            if (metrics.rtt_ms == 0.0) {
                                guint sent_rb_lsr = 0;
                                if (gst_structure_get_uint(src_stats, "sent-rb-lsr", &sent_rb_lsr) && sent_rb_lsr > 0) {
                                    // LSR 정보가 존재한다면 리포트는 오가고 있음을 의미
                                }
                            }

                            gst_structure_free(stats_struct);
                            return metrics; // 가장 유효한(패킷이 있는) 세션 정보 반환
                        }
                    }
                }
                gst_structure_free(stats_struct);
            }
        }
        return metrics;
    }

private:
    void bind_rtpbin(GstElement* rtpbin) {
        g_signal_connect(rtpbin, "on-ssrc-active", G_CALLBACK(on_ssrc_active), this);
    }

    static void on_new_manager(GstElement* src, GstElement* manager, gpointer data) {
        auto* self = static_cast<GstStatsCollector*>(data);
        self->bind_rtpbin(manager);
    }

    static void on_ssrc_active(GstElement* rtpbin, guint sessid, guint ssrc, gpointer data) {
        auto* self = static_cast<GstStatsCollector*>(data);
        
        std::lock_guard<std::mutex> lock(self->mutex_);
        GstElement *session_elem = nullptr;
        g_signal_emit_by_name(rtpbin, "get-internal-session", sessid, &session_elem);
        
        if (session_elem) {
            bool exists = false;
            for (auto* s : self->rtp_sessions_) {
                if (s == G_OBJECT(session_elem)) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                self->rtp_sessions_.push_back(G_OBJECT(session_elem));
                std::cout << "📈 [Stats] New RTP Session attached: SessID=" << sessid << " (Total: " << self->rtp_sessions_.size() << ")" << std::endl;
            } else {
                g_object_unref(session_elem);
            }
        }
    }

    std::vector<GObject*> rtp_sessions_;
    std::mutex mutex_;
};

#endif // GST_STATS_COLLECTOR_HPP
