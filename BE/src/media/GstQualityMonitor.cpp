#include "media/GstQualityMonitor.hpp"

GST_DEBUG_CATEGORY_STATIC(gst_quality_monitor_debug);
#define GST_CAT_DEFAULT gst_quality_monitor_debug

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE("sink",
    GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE("src",
    GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

G_DEFINE_TYPE(GstQualityMonitor, gst_quality_monitor, GST_TYPE_ELEMENT);

/* 패킷이 들어왔을 때 단순히 통과시키는 함수 */
static GstFlowReturn gst_quality_monitor_chain(GstPad *pad, GstObject *parent, GstBuffer *buf) {
    GstQualityMonitor *self = GST_QUALITY_MONITOR(parent);
    return gst_pad_push(self->srcpad, buf);
}

static void gst_quality_monitor_dispose(GObject *object) {
    GstQualityMonitor *self = GST_QUALITY_MONITOR(object);
    if (self->collector) {
        delete self->collector;
        self->collector = nullptr;
    }
    G_OBJECT_CLASS(gst_quality_monitor_parent_class)->dispose(object);
}

static GstStateChangeReturn gst_quality_monitor_change_state(GstElement *element, GstStateChange transition) {
    GstQualityMonitor *self = GST_QUALITY_MONITOR(element);

    if (transition == GST_STATE_CHANGE_NULL_TO_READY) {
        // [FIX] Initialize collector only if not already present and fix parent leak
        if (!self->collector) {
            self->collector = new GstStatsCollector();
            GstObject* parent = gst_object_get_parent(GST_OBJECT(element));
            if (parent) {
                self->collector->attach(GST_ELEMENT(parent));
                gst_object_unref(parent); // Release the ref returned by gst_object_get_parent
            }
        }
    }

    return GST_ELEMENT_CLASS(gst_quality_monitor_parent_class)->change_state(element, transition);
}

static void gst_quality_monitor_class_init(GstQualityMonitorClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *gstelement_class = GST_ELEMENT_CLASS(klass);

    gobject_class->dispose = gst_quality_monitor_dispose;
    gstelement_class->change_state = gst_quality_monitor_change_state;

    gst_element_class_add_pad_template(gstelement_class, gst_static_pad_template_get(&src_template));
    gst_element_class_add_pad_template(gstelement_class, gst_static_pad_template_get(&sink_template));

    gst_element_class_set_static_metadata(gstelement_class,
        "Quality Monitor", "Generic/Network",
        "Extracts RTP/RTCP quality metrics", "rokgeun");
}

static void gst_quality_monitor_init(GstQualityMonitor *self) {
    self->sinkpad = gst_pad_new_from_static_template(&sink_template, "sink");
    gst_pad_set_chain_function(self->sinkpad, GST_DEBUG_FUNCPTR(gst_quality_monitor_chain));
    GST_PAD_SET_PROXY_CAPS(self->sinkpad);
    gst_element_add_pad(GST_ELEMENT(self), self->sinkpad);

    self->srcpad = gst_pad_new_from_static_template(&src_template, "src");
    GST_PAD_SET_PROXY_CAPS(self->srcpad);
    gst_element_add_pad(GST_ELEMENT(self), self->srcpad);

    self->collector = nullptr;
}

/* 엘리먼트를 'qualitymonitor'라는 이름으로 등록 */
gboolean gst_quality_monitor_register(GstPlugin *plugin) {
    return gst_element_register(plugin, "qualitymonitor", GST_RANK_NONE, GST_TYPE_QUALITY_MONITOR);
}
