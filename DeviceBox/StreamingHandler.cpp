#include "StreamingHandler.h"

#include <cassert>

#include <gio/gio.h>
#include <gst/gst.h>

#include <CxxPtr/GlibPtr.h>
#include <CxxPtr/GstPtr.h>

#include "Common/Config.h"
#include "finally_execute.h"
#include "Log.h"
#include "CertificateProvider.h"


namespace DeviceBox
{

struct Streamer
{
public:
    static inline const std::shared_ptr<spdlog::logger>& Log();

    Streamer(
        asio::io_service*,
        const SourceConfig&,
        GTlsCertificate* clientCertificate,
        const std::function<void ()>& safeToDestroy);
    ~Streamer();

    bool valid() const;

    void stream(const std::string& destination,
                const std::function<void ()>& streaming,
                const std::function<void ()>& streamFailed);
    void stopStream();

private:
    static gboolean DecodebinAutoplugContinue(
        GstElement*, GstPad*,
        GstCaps*, Streamer*);

    static void DecodebinPadAdded(
        GstElement* ,GstPad*,
        Streamer*);

    void initPipeline();
    void linkToRtspSink(GstPad* pad);

    static GstBusSyncReply threadedBusMessage(
        GstBus* bus, GstMessage* message, gpointer userData);

    void asyncPlaying();
    void asyncError();
    void asyncEos();

    void noMoreRefs();

private:
    RefCounter<Streamer, &Streamer::noMoreRefs> _thisRefCounter;

    asio::io_service* _ioService;

    SourceConfig _config;
    GTlsCertificate *const _clientCertificate;

    const std::function<void ()> _safeToDestroy;

    std::function<void ()> _streaming;
    std::function<void ()> _streamFailed;

    GstCapsPtr _supportedCaps;

    GstElementPtr _pipeline;
    GstElement* _decodebin;
    GstElement* _rtspsink;
};

const std::shared_ptr<spdlog::logger>& Streamer::Log()
{
    return DeviceBox::StreamingLog();
}

Streamer::Streamer(
    asio::io_service* ioService,
    const SourceConfig& config,
    GTlsCertificate* clientCertificate,
    const std::function<void ()>& safeToDestroy) :
    _thisRefCounter(this),
    _ioService(ioService),
    _config(config),
    _clientCertificate(clientCertificate),
    _safeToDestroy(safeToDestroy),
    _supportedCaps(nullptr),
    _decodebin(nullptr), _rtspsink(nullptr)
{
    Log()->trace(">> Streamer::Streamer");

    static const bool gstreamerInitDone =
        gst_init_check(0, nullptr, nullptr);
    assert(gstreamerInitDone);

    initPipeline();
}

Streamer::~Streamer()
{
    Log()->trace(">> Streamer::~Streamer");

    assert(!_pipeline);
    if(_pipeline)
        gst_element_set_state(_pipeline.get(), GST_STATE_NULL);
}

gboolean Streamer::DecodebinAutoplugContinue(
    GstElement* /*uridecodebin*/, GstPad* /*pad*/, GstCaps* caps,
    Streamer* self)
{
    if(gst_caps_is_always_compatible(caps, self->_supportedCaps.get())) {
        return FALSE;
    } else {
        return TRUE;
    }
}

void Streamer::linkToRtspSink(GstPad* pad)
{
    GstPadTemplate* sinkPadTemplate =
        gst_element_class_get_pad_template(
#if GST_CHECK_VERSION(1, 13, 90)
            GST_ELEMENT_GET_CLASS(_rtspsink), "sink_%u");
#else
            GST_ELEMENT_GET_CLASS(_rtspsink), "stream_%u");
#endif
    GstPad* sinkPad =
        gst_element_request_pad(
            _rtspsink, sinkPadTemplate, nullptr, nullptr);
    finally_execute unrefSinkPadTemplate =
        make_fin_exec(std::bind(gst_object_unref, sinkPad));

    if(GST_PAD_LINK_OK != gst_pad_link(pad, sinkPad))
        assert(false);
}

void Streamer::DecodebinPadAdded(
    GstElement* /*uridecodebin*/, GstPad* pad,
    Streamer* self)
{
    GstCapsPtr capsPtr(gst_pad_query_caps(pad, nullptr));
    GstCaps* caps =capsPtr.get();

    GstElement* pipeline = self->_pipeline.get();
    GstCaps* supportedCaps = self->_supportedCaps.get();

    bool skip = true;
    if(gst_caps_is_always_compatible(caps, supportedCaps)) {
        GstElementPtr parsePtr(gst_element_factory_make("h264parse", nullptr));
        GstElement* parse = parsePtr.get();
        if(!parse)
            Log()->critical("Fail to create \"h264parse\" element");

        if(parse) {
            //g_object_set(parse, "config-interval", 1, nullptr);

            gst_bin_add(GST_BIN(pipeline), parsePtr.release());
            gst_element_sync_state_with_parent(parse);

            GstPad* parseSinkPad = gst_element_get_static_pad(parse, "sink");
            gst_pad_link(pad, parseSinkPad);
            gst_object_unref(parseSinkPad);

            GstPad* parseSrcPad = gst_element_get_static_pad(parse, "src");
            finally_execute unrefParseSrcPad =
                make_fin_exec(std::bind(gst_object_unref, parseSrcPad));

            self->linkToRtspSink(parseSrcPad);

            skip = false;
        }
    }

    if(skip) {
        GstElementPtr fakesinkPtr(gst_element_factory_make("fakesink", nullptr));
        GstElement* fakesink = fakesinkPtr.get();
        if(!fakesink)
            Log()->critical("Fail to create \"fakesink\" element");

        if(fakesink) {
            gst_bin_add(GST_BIN(pipeline), fakesinkPtr.release());
            gst_element_sync_state_with_parent(fakesink);

            GstPadPtr sinkPadPtr(gst_element_get_static_pad(fakesink, "sink"));
            GstPad* sinkPad = sinkPadPtr.get();

            if(GST_PAD_LINK_OK != gst_pad_link(pad, sinkPad))
                assert(false);
        }
    }
}

GstBusSyncReply Streamer::threadedBusMessage(
    GstBus* bus,
    GstMessage* message,
    gpointer userData)
{
    Streamer* self = static_cast<Streamer*>(userData);

    // Log()->trace("Streamer message: {}", GST_MESSAGE_TYPE_NAME(message));

    switch(GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_STATE_CHANGED: {
            if(GST_ELEMENT(message->src) == self->_pipeline.get()) {
                GstState newState;
                gst_message_parse_state_changed(message, nullptr, &newState, nullptr);
                switch(newState) {
                    case GST_STATE_PLAYING:
                        self->_ioService->post(
                            std::bind(&Streamer::asyncPlaying, self->_thisRefCounter));
                        break;
                    default:
                        break;
                }
            }
            break;
        }
        case GST_MESSAGE_EOS:
            Log()->debug("GStreamer: EOS");

            self->_ioService->post(
                std::bind(&Streamer::asyncEos, self->_thisRefCounter));
            break;
        case GST_MESSAGE_ERROR: {
            GError* error = nullptr;
            gchar* debugInfo = nullptr;
            gst_message_parse_error(message, &error, &debugInfo);
            Log()->error("GStreamer. {}: {}", GST_ELEMENT_NAME(message->src), error->message);
            g_error_free(error);
            g_free(debugInfo);

            self->_ioService->post(
                std::bind(&Streamer::asyncError, self->_thisRefCounter));
            break;
        }
        default:
            break;
    }

    return GST_BUS_PASS;
}

void Streamer::initPipeline()
{
    Log()->trace(">> Streamer::initPipeline");

    _supportedCaps.reset(gst_caps_from_string("video/x-h264"));

    _pipeline.reset(gst_pipeline_new(nullptr));
    GstElement* pipeline = _pipeline.get();

    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    gst_bus_set_sync_handler(bus, threadedBusMessage, this, nullptr);
    gst_object_unref(bus);

    GstElementPtr decodebinPtr(gst_element_factory_make("uridecodebin", nullptr));
    GstElement* decodebin = decodebinPtr.get();
    if(!decodebin)
        Log()->critical("Fail to create \"uridecodebin\" element");

    CertificateProviderPtr certificateProviderPtr(
        certificate_provider_new(_clientCertificate));
    CertificateProvider* certificateProvider =certificateProviderPtr.get();
    if(!certificateProvider)
        Log()->critical("Fail to create \"CertificateProvider\" object");

    GstElementPtr rtspsinkPtr(gst_element_factory_make("rtspclientsink", nullptr));
    GstElement* rtspsink = rtspsinkPtr.get();
    if(!rtspsink)
        Log()->critical("Fail to create \"rtspclientsink\" element");

    if(decodebin && rtspsink && certificateProvider) {
        g_object_set(rtspsink, "tls-interaction", certificateProvider, nullptr);
#if DISABLE_VERIFY_RESTREAM_SERVER
        g_object_set(rtspsink, "tls-validation-flags", 0, nullptr);
#endif

        GstCaps* decodebinCaps = nullptr;
        g_object_get(decodebin, "caps", &decodebinCaps, nullptr);
        GstCapsPtr decodebinCapsPtr(decodebinCaps);

        GstCapsPtr desiredCapsPtr(gst_caps_copy(decodebinCaps));
        GstCaps* desiredCaps = desiredCapsPtr.get();
        gst_caps_append(desiredCaps, gst_caps_copy(_supportedCaps.get()));

        g_object_set(decodebin, "caps", desiredCaps, nullptr);

        g_signal_connect(
            decodebin, "autoplug-continue",
            G_CALLBACK(DecodebinAutoplugContinue), this);
        g_signal_connect(
            decodebin, "pad-added",
            G_CALLBACK(DecodebinPadAdded), this);

        gst_bin_add_many(
            GST_BIN(pipeline),
            decodebinPtr.release(), rtspsinkPtr.release(), nullptr);

        _decodebin = decodebin;
        _rtspsink = rtspsink;
    } else
        _pipeline.reset();
}

bool Streamer::valid() const
{
    return nullptr != _pipeline;
}

void Streamer::stream(
    const std::string& destination,
    const std::function<void ()>& streaming,
    const std::function<void ()>& streamFailed)
{
    Log()->trace(">> Streamer::stream");

    _streaming = streaming;
    _streamFailed = streamFailed;

    GstElement* pipeline = _pipeline.get();

#ifndef NDEBUG
    GstState state;
    gst_element_get_state(pipeline, &state, nullptr, 0);
    assert(GST_STATE_NULL == state);
#endif

    g_object_set(_decodebin, "uri", _config.uri.c_str(), nullptr);
    g_object_set(_rtspsink,
                 "location", destination.c_str(),
                 nullptr);

    switch(gst_element_set_state(pipeline, GST_STATE_PLAYING)) {
        case GST_STATE_CHANGE_FAILURE:
            Log()->error(
                "Streaming failed. Source: {}, destination: {}",
                _config.uri, destination);
            _ioService->post(_streamFailed);
            break;
        case GST_STATE_CHANGE_ASYNC:
            Log()->debug(
                "Streaming starting. Source: {}, destination: {}",
                _config.uri, destination);
            return;
        case GST_STATE_CHANGE_SUCCESS:
        case GST_STATE_CHANGE_NO_PREROLL:
            Log()->debug(
                "Streaming started. Source: {}, destination: {}",
                _config.uri, destination);
            _ioService->post(_streaming);
            break;
    }
}

void Streamer::stopStream()
{
    Log()->trace(">> Streamer::stopStream");

    if(_pipeline) {
        GstElement* pipeline = _pipeline.get();

        switch(gst_element_set_state(pipeline, GST_STATE_NULL)) {
            case GST_STATE_CHANGE_FAILURE:
            case GST_STATE_CHANGE_NO_PREROLL:
            case GST_STATE_CHANGE_ASYNC:
                assert(false);
                break;
            case GST_STATE_CHANGE_SUCCESS:
                Log()->debug(
                    "Streaming finished");
                break;
        }

        _pipeline.reset();
    } else
        Log()->trace("Pipeline was reseted already");

    if(!_thisRefCounter.hasRefs())
        _ioService->post(_safeToDestroy);
    else
        Log()->trace("Refs count: {}", _thisRefCounter.refsCount());
}

void Streamer::asyncPlaying()
{
    Log()->trace(">> Streamer::asyncPlaying");

    if(_streaming)
        _ioService->post(_streaming);
}

void Streamer::asyncError()
{
    Log()->trace(">> Streamer::error");

    _ioService->post(_streamFailed);

    stopStream();
}

void Streamer::asyncEos()
{
    Log()->trace(">> Streamer::eos");

    _ioService->post(_streamFailed);

    stopStream();
}

void Streamer::noMoreRefs()
{
    Log()->trace(">> Streamer::noMoreRefs");

    if(!_pipeline)
        _ioService->post(_safeToDestroy);
}


struct StreamingHandler::Private
{
    asio::io_service* ioService;

    SourceConfig config;
    const AuthConfig *const authConfig;

    GTlsCertificatePtr clientCertificate;

    std::unique_ptr<Streamer> streamer;

    std::function<void ()> shuttedDown;
};


StreamingHandler::StreamingHandler(
    asio::io_service* ioService,
    const SourceConfig& config,
    const AuthConfig* authConfig) :
    _thisRefCounter(this),
    _p(new Private{ioService, config, authConfig})
{
    Streamer::Log()->trace(">> StreamingHandler::StreamingHandler, SourceId: {}", config.id);

    GError* error = nullptr;
    _p->clientCertificate.reset(
        g_tls_certificate_new_from_pem(
            authConfig->certificate.data(),
            -1, &error));

    if(!_p->clientCertificate)
      Streamer::Log()->error("failed to parse PEM: {}", error->message);
}

StreamingHandler::~StreamingHandler()
{
    Streamer::Log()->trace(">> StreamingHandler::~StreamingHandler, SourceId: {}", _p->config.id);

    _p.reset();
}

void StreamingHandler::streaming(const std::function<void ()>& outerStreaming)
{
    Streamer::Log()->trace(">> StreamingHandler::streaming");

    if(outerStreaming)
        _p->ioService->post(outerStreaming);
}

void StreamingHandler::streamFailed(const std::function<void ()>& outerStreamFailed)
{
    Streamer::Log()->trace(">> StreamingHandler::streamFailed");

    if(outerStreamFailed)
        _p->ioService->post(outerStreamFailed);
}

void StreamingHandler::destroyStreaming()
{
    Streamer::Log()->trace(">> StreamingHandler::destroyStreaming");

    _p->streamer.reset();

    if(_p->shuttedDown) {
        _p->ioService->post(_p->shuttedDown);
        _p->shuttedDown = decltype(_p->shuttedDown)();
    }
}

void StreamingHandler::stream(
    const std::string& destination,
    const std::function<void ()>& streaming,
    const std::function<void ()>& streamFailed)
{
    Streamer::Log()->trace(">> StreamingHandler::stream");

    if(_p->streamer) {
        Streamer::Log()->warn("Streamer is active already");
        return;
    }

    if(!_p->clientCertificate)
      Streamer::Log()->error("Can't start streaming. Client certificate missing.");

    _p->streamer.reset(
        new Streamer(
            _p->ioService, _p->config, _p->clientCertificate.get(),
            std::bind(&StreamingHandler::destroyStreaming, _thisRefCounter)));

    if(!_p->streamer->valid()) {
        Streamer::Log()->critical("Streamer init failed");
        _p->streamer.reset();
        _p->ioService->post(streamFailed);
        return;
    }

    _p->streamer->stream(
        destination,
        std::bind(&StreamingHandler::streaming, _thisRefCounter, streaming),
        std::bind(&StreamingHandler::streamFailed, _thisRefCounter, streamFailed));
}

void StreamingHandler::stopStream()
{
    Streamer::Log()->trace(">> StreamingHandler::stopStream");

    if(_p->streamer)
        _p->streamer->stopStream();
}

bool StreamingHandler::active() const
{
    return _thisRefCounter.hasRefs();
}

void StreamingHandler::shutdown(const std::function<void ()>& finished)
{
    Streamer::Log()->trace(">> StreamingHandler::shutdown");

    if(_p->streamer) {
        assert(!_p->shuttedDown);
        _p->shuttedDown = finished;
        _p->streamer->stopStream();
    } else
        _p->ioService->post(finished);
}

}
