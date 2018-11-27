#include "SplitHandler.h"

#include <cassert>
#include <functional>

#include <gst/gst.h>

#include <CxxPtr/GstPtr.h>

#include "finally_execute.h"
#include "Log.h"


namespace DeviceBox
{

struct SplitHandler::Private
{
    static inline const std::shared_ptr<spdlog::logger>& Log();

    Private(asio::io_service*, const SourceConfig&);

    void initPipeline();
    void startSplit(
        const std::function<void (
            const std::string& dir,
            const std::string& name)>& fileReady);
    void stopSplit(const std::function<void ()>& finished);
    void shutdown(const std::function<void ()>& finished);

public:
    asio::io_service* ioService;

    SourceConfig config;

    std::function<void (
        const std::string& dir,
        const std::string& name)> fileReadyCallback;

    GstCapsPtr supportedCaps;

    GstElementPtr pipeline;
    GstElement* decodebin;
    GstElement* filesink;
    GstElement* splitmuxsink;

private:
    static gboolean DecodebinAutoplugContinue(
        GstElement*, GstPad*,
        GstCaps*, Private*);

    static void DecodebinPadAdded(
        GstElement* ,GstPad*,
        SplitHandler::Private*);

    static void OnFilesinkStateChanged(
        GstBus*, GstMessage*, SplitHandler::Private*);
};

const std::shared_ptr<spdlog::logger>& SplitHandler::Private::Log()
{
    return DeviceBox::SplittingLog();
}

SplitHandler::Private::Private(asio::io_service* ioService, const SourceConfig& config) :
    ioService(ioService),
    config(config),
    decodebin(nullptr), filesink(nullptr), splitmuxsink(nullptr)
{
    static const bool gstreamerInitDone =
        gst_init_check(0, nullptr, nullptr);
    assert(gstreamerInitDone);

    initPipeline();
}

gboolean SplitHandler::Private::DecodebinAutoplugContinue(
    GstElement* /*uridecodebin*/, GstPad* /*pad*/, GstCaps* caps,
    SplitHandler::Private* self)
{
    GstCaps* supportedCaps = self->supportedCaps.get();

    if(gst_caps_is_always_compatible(caps, supportedCaps)) {
        return FALSE;
    } else {
        return TRUE;
    }
}

void SplitHandler::Private::DecodebinPadAdded(
    GstElement* /*uridecodebin*/, GstPad* pad,
    SplitHandler::Private* self)
{
    GstCaps* supportedCaps = self->supportedCaps.get();

    GstElement* pipeline = self->pipeline.get();

    GstCaps* caps = gst_pad_query_caps(pad, nullptr);
    finally_execute unrefCaps =
        make_fin_exec(std::bind(gst_caps_unref, caps));

    if(gst_caps_is_always_compatible(caps, supportedCaps)) {
        GstElement* parse = gst_element_factory_make("h264parse", nullptr);

        gst_bin_add(GST_BIN(pipeline), parse);
        gst_element_sync_state_with_parent(parse);

        GstPad* parseSinkPad = gst_element_get_static_pad(parse, "sink");
        gst_pad_link(pad, parseSinkPad);
        gst_object_unref(parseSinkPad);

        GstPad* parseSrcPad = gst_element_get_static_pad(parse, "src");
        finally_execute unrefParseSrcPad =
            make_fin_exec(std::bind(gst_object_unref, parseSrcPad));

        GstPadTemplate* splitMuxSinkPadTemplate =
            gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(self->splitmuxsink), "video");
        GstPad* splitmuxSinkPad =
            gst_element_request_pad(self->splitmuxsink, splitMuxSinkPadTemplate, nullptr, nullptr);
        finally_execute unrefSplitmuxSinkPad =
            make_fin_exec(std::bind(gst_object_unref, splitmuxSinkPad));

        if(GST_PAD_LINK_OK != gst_pad_link(parseSrcPad, splitmuxSinkPad))
            assert(false);
    } else {
        GstElement* fakesink = gst_element_factory_make("fakesink", nullptr);
        gst_bin_add(GST_BIN(pipeline), fakesink);
        gst_element_sync_state_with_parent(fakesink);

        GstPad* sinkPad = gst_element_get_static_pad(fakesink, "sink");
        finally_execute unrefSinkPad =
            make_fin_exec(std::bind(gst_object_unref, sinkPad));
        if(GST_PAD_LINK_OK != gst_pad_link(pad, sinkPad))
            assert(false);
    }
}

void SplitHandler::Private::OnFilesinkStateChanged(
    GstBus* bus, GstMessage* message, SplitHandler::Private* self)
{
    if(GST_MESSAGE_TYPE(message) != GST_MESSAGE_STATE_CHANGED)
        return;

    if(GST_OBJECT(self->filesink) != message->src)
        return;

    GstState newState;
    gst_message_parse_state_changed(message, nullptr, &newState, nullptr);

    if(newState != GST_STATE_NULL)
        return;


    gchar* location;
    g_object_get(message->src, "location", &location, nullptr);
    if(!location)
        return;

    gchar* dir = g_path_get_dirname(location);
    gchar* name = g_path_get_basename(location);
    g_free(location);

    self->ioService->post(
        std::bind(
            self->fileReadyCallback,
            std::string(dir), std::string(name)));

    g_free(dir);
    g_free(name);
}

void SplitHandler::Private::initPipeline()
{
    supportedCaps.reset(gst_caps_from_string("video/x-h264"));
    GstCaps* supportedCaps = this->supportedCaps.get();

    this->pipeline.reset(gst_pipeline_new(nullptr));
    GstElement* pipeline = this->pipeline.get();

    GstElementPtr decodebinPtr(gst_element_factory_make("uridecodebin", nullptr));
    GstElement* decodebin  = decodebinPtr.get();
    if(!decodebin)
        Log()->critical("Fail to create \"uridecodebin\" element");

    GstElementPtr mpegtsmuxPtr(gst_element_factory_make("mpegtsmux", nullptr));
    GstElement* mpegtsmux = mpegtsmuxPtr.get();
    if(!mpegtsmux)
        Log()->critical("Fail to create \"mpegtsmux\" element");

    GstElementPtr filesinkPtr(gst_element_factory_make("filesink", nullptr));
    filesink = filesinkPtr.get();
    if(!filesink)
        Log()->critical("Fail to create \"filesink\" element");

    GstElementPtr splitmuxsinkPtr(gst_element_factory_make("splitmuxsink", nullptr));
    splitmuxsink = splitmuxsinkPtr.get();
    if(!splitmuxsink)
        Log()->critical("Fail to create \"splitmuxsink\" element");

    if(decodebin && mpegtsmux && filesink && splitmuxsink) {
        GstCaps* decodebinCaps = nullptr;
        g_object_get(decodebin, "caps", &decodebinCaps, nullptr);
        finally_execute unrefDecodebinCaps =
            make_fin_exec(std::bind(gst_caps_unref, decodebinCaps));

        GstCaps* desiredCaps = gst_caps_copy(decodebinCaps);
        finally_execute unrefDesiredCaps =
            make_fin_exec(std::bind(gst_caps_unref, desiredCaps));
        gst_caps_append(desiredCaps, gst_caps_copy(supportedCaps));

        g_object_set(decodebin, "caps", desiredCaps, nullptr);

        g_signal_connect(decodebin, "autoplug-continue", G_CALLBACK(DecodebinAutoplugContinue), this);
        g_signal_connect(decodebin, "pad-added", G_CALLBACK(DecodebinPadAdded), this);

        g_object_set(splitmuxsink,
                     "muxer", mpegtsmux,
                     "sink", filesink,
                     "location", (config.archivePath + "/%010d.ts").c_str(),
                     "max-size-bytes", static_cast<guint64>(config.desiredFileSize),
                     nullptr);

        GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
        finally_execute unrefBus =
            make_fin_exec(std::bind(gst_object_unref, bus));
        gst_bus_enable_sync_message_emission(bus);
        g_signal_connect(bus, "sync-message::state-changed", G_CALLBACK(OnFilesinkStateChanged), this);

        gst_bin_add_many(
            GST_BIN(pipeline),
            decodebinPtr.release(), splitmuxsinkPtr.release(), nullptr);

        this->decodebin = decodebin;
        mpegtsmuxPtr.release();
        this->filesink = filesinkPtr.release();
        this->splitmuxsink = splitmuxsinkPtr.release();
    } else
        this->pipeline.reset();
}

void SplitHandler::Private::startSplit(
    const std::function<void (
        const std::string& dir,
        const std::string& name)>& fileReady)
{
    if(!pipeline) {
        Log()->error("Can't start split. Pipeline not initialized.");
        return;
    }

    GstElement* pipeline = this->pipeline.get();

    fileReadyCallback = fileReady;

    g_object_set(decodebin, "uri", config.uri.c_str(), nullptr);

    switch(gst_element_set_state(pipeline, GST_STATE_PLAYING)) {
        case GST_STATE_CHANGE_FAILURE:
            assert(false);
            break;
        case GST_STATE_CHANGE_ASYNC:
        case GST_STATE_CHANGE_SUCCESS:
        case GST_STATE_CHANGE_NO_PREROLL:
            break;
    }
}

void SplitHandler::Private::stopSplit(const std::function<void ()>& finished)
{
    if(!pipeline) {
        ioService->post(finished);
        return;
    }

    GstElement* pipeline = this->pipeline.get();

    switch(gst_element_set_state(pipeline, GST_STATE_NULL)) {
        case GST_STATE_CHANGE_FAILURE:
            assert(false);
            break;
        case GST_STATE_CHANGE_ASYNC:
        case GST_STATE_CHANGE_SUCCESS:
        case GST_STATE_CHANGE_NO_PREROLL:
            break;
    }

    ioService->post(finished);
}

void SplitHandler::Private::shutdown(const std::function<void ()>& finished)
{
    stopSplit(finished);
    this->pipeline.reset();
}

SplitHandler::SplitHandler(asio::io_service* io_service, const SourceConfig& config) :
    _thisRefCounter(this), _p(new Private(io_service, config))
{
    _p->initPipeline();
    if(!_p->pipeline)
        _p->Log()->error("Splitter init failed");
}

SplitHandler::~SplitHandler()
{
    assert(!_p->pipeline);
}

const SourceConfig& SplitHandler::config() const
{
    return _p->config;
}

void SplitHandler::startSplit(
    const std::function<void (
        const std::string& dir,
        const std::string& name)>& fileReady)
{
    if(_p->pipeline)
        _p->startSplit(fileReady);
}

bool SplitHandler::active() const
{
    return _thisRefCounter.hasRefs();
}

void SplitHandler::shutdown(const std::function<void ()>& finished)
{
    _p->shutdown(finished);
}

}
