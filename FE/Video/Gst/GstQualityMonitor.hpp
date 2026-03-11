#ifndef GST_QUALITY_MONITOR_HPP
#define GST_QUALITY_MONITOR_HPP

#include <gst/gst.h>
#include "GstStatsCollector.hpp"

/* GObject 정의를 위한 표준 매크로 */
#define GST_TYPE_QUALITY_MONITOR (gst_quality_monitor_get_type())
G_DECLARE_FINAL_TYPE(GstQualityMonitor, gst_quality_monitor, GST, QUALITY_MONITOR, GstElement)

struct _GstQualityMonitor {
    GstElement parent;
    GstPad *sinkpad, *srcpad;
    GstStatsCollector* collector;
};

/* 엘리먼트 등록 함수 */
gboolean gst_quality_monitor_register(GstPlugin *plugin);

#endif
