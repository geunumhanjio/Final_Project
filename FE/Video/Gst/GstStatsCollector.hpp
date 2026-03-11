#ifndef GST_STATS_COLLECTOR_HPP
#define GST_STATS_COLLECTOR_HPP

#include <gst/gst.h>
#include <mutex>
#include <string>
#include <iostream>
#include <iomanip>

struct RtpQualityMetrics {
    uint64_t packets_received = 0;
    int32_t  packets_lost = 0;
    double   jitter_ms = 0.0;
    uint64_t bytes_received = 0;
    double   rtt_ms = 0.0;
    uint64_t frames_received = 0; // RTP Marker bit based frame count
};

class GstStatsCollector {
public:
    GstStatsCollector() : rtp_session_(nullptr), pipeline_(nullptr), frames_count_(0) {}
    ~GstStatsCollector() {
        if (rtp_session_) {
            g_object_unref(rtp_session_);
        }
        if (pipeline_) {
            gst_object_unref(pipeline_);
        }
    }

    void incrementFrames() {
        std::lock_guard<std::mutex> lock(mutex_);
        frames_count_++;
    }

    void attach(GstElement* pipeline) {
        if (pipeline_) gst_object_unref(pipeline_);
        pipeline_ = (GstElement*)gst_object_ref(pipeline);

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
        metrics.frames_received = frames_count_; 

        if (!rtp_session_) return metrics;

        GstStructure *stats_struct = nullptr;
        g_object_get(rtp_session_, "stats", &stats_struct, nullptr);

        if (stats_struct) {
            const GValue *val = gst_structure_get_value(stats_struct, "source-stats");
            if (val && G_VALUE_HOLDS(val, G_TYPE_VALUE_ARRAY)) {
                GValueArray *arr = (GValueArray *)g_value_get_boxed(val);
                for (guint i = 0; i < arr->n_values; ++i) {
                    const GValue *sval = g_value_array_get_nth(arr, i);
                    GstStructure *src_stats = (GstStructure *)g_value_get_boxed(sval);
                    if (!src_stats) continue;

                    gboolean internal = FALSE;
                    gboolean is_sender = FALSE;
                    gst_structure_get_boolean(src_stats, "internal", &internal);
                    gst_structure_get_boolean(src_stats, "is-sender", &is_sender);

                    if (!internal && is_sender) {
                        uint64_t pkts = 0, bytes = 0;
                        int32_t lost = 0;
                        guint jitter_units = 0;
                        gint clock_rate = 90000;

                        gst_structure_get_uint64(src_stats, "packets-received", &pkts);
                        gst_structure_get_int(src_stats, "packets-lost", &lost);
                        gst_structure_get_uint64(src_stats, "bytes-received", &bytes);
                        gst_structure_get_uint(src_stats, "jitter", &jitter_units);
                        gst_structure_get_int(src_stats, "clock-rate", &clock_rate);

                        metrics.packets_received = pkts;
                        metrics.packets_lost = lost;
                        metrics.bytes_received = bytes;
                        metrics.jitter_ms = (clock_rate > 0) ? (jitter_units * 1000.0 / clock_rate) : 0.0;
                        
                        // We check rb-round-trip just in case, but VideoWidget prefers Pinger
                        guint rb_round_trip = 0;
                        if (gst_structure_get_uint(src_stats, "rb-round-trip", &rb_round_trip) && rb_round_trip > 0) {
                            double rtt_seconds = (rb_round_trip >> 16) + static_cast<double>(rb_round_trip & 0xFFFF) / 65536.0;
                            metrics.rtt_ms = rtt_seconds * 1000.0;
                        }
                        break;
                    }
                }
            }
            gst_structure_free(stats_struct);
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
        if (sessid != 0) return; 

        std::lock_guard<std::mutex> lock(self->mutex_);
        if (!self->rtp_session_) {
            GstElement *session_elem = nullptr;
            g_signal_emit_by_name(rtpbin, "get-internal-session", sessid, &session_elem);
            if (session_elem) {
                self->rtp_session_ = G_OBJECT(session_elem); 
            }
        }
    }

    GObject* rtp_session_;
    GstElement* pipeline_;
    uint64_t frames_count_; 
    std::mutex mutex_;
};

#endif // GST_STATS_COLLECTOR_HPP
