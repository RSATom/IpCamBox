#include "Controller.h"

#include "Config.h"


namespace DeviceBox
{

///////////////////////////////////////////////////////////////////////////////
Controller::SourceHandlers::SourceHandlers(
    asio::io_service* ioService,
    const SourceConfig& config,
    Dropbox* dropbox,
    const AuthConfig* authConfig) :
    splitter(ioService, config),
    streamer(ioService, config, authConfig),
    dropboxFolder(ioService, dropbox)
{
}

///////////////////////////////////////////////////////////////////////////////
const std::shared_ptr<spdlog::logger>& Controller::Log()
{
    return ControllerLog();
}

Controller::Controller(asio::io_service* ioService, const AuthConfig& authConfig) :
    _ioService(ioService),
    _working(new asio::io_service::work(*ioService)),
    _authConfig(authConfig),
    _dropbox(ioService),
    _shrinkTimer(*ioService)
{
}

Controller::~Controller()
{
    assert(_handlers.empty());
}

const AuthConfig* Controller::authConfig() const
{
    return &_authConfig;
}

void Controller::newFileAvailable(
    const SplitHandler* handler,
    const std::string& dir, const std::string& name)
{
    const SourceConfig& config = handler->config();

    if(!config.dropboxMaxStorage)
        return;

    const std::string localFile = dir + "/" + name;
    const std::string cloudFile = config.dropboxArchivePath + name;
    _dropbox.upload(localFile, cloudFile,
        [localFile] (long responseCode, const std::string& response) {
            remove(localFile.c_str());
        }
    );
}

void Controller::startHandleSource(const SourceConfig& config)
{
    Log()->trace(">> Controller::startHandleSource");

    SourceHandlers& handlers =
        _handlers.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(config.id),
            std::forward_as_tuple(_ioService, config, &_dropbox, authConfig())).first->second;

    if(config.dropboxMaxStorage) {
        handlers.dropboxFolder.startSync(config.dropboxArchivePath);

        handlers.splitter.startSplit(
            std::bind(
                &Controller::newFileAvailable, this,
                &handlers.splitter,
                std::placeholders::_1, std::placeholders::_2)
        );

        scheduleShrinkStorage();
    }
}

void Controller::removeSource(const SourceId& source, const std::function<void ()>& finished)
{
    Log()->debug("Removing source {}", source);

    auto it = _handlers.find(source);

    assert(_handlers.end() != it);

    if(_handlers.end() != it) {
        const SourceHandlers& handlers = it->second;
        assert(!handlers.streamer.active() &&
               !handlers.splitter.active() &&
               !handlers.dropboxFolder.active());
        _handlers.erase(it);
    }

    if(_handlers.empty())
        _ioService->post(finished);
}

void Controller::stopHandleSources(const std::function<void ()>& finished)
{
    Log()->trace(">> Controller::stopHandleSources");

    if(!_handlers.empty()) {
        for(auto& pair: _handlers) {
            const SourceId& sourceId = pair.first;
            SourceHandlers& handlers = pair.second;

            Log()->debug("Shutting down {}", sourceId);

            auto streamerShuttedDown =
                [this, sourceId, finished] () {
                    Log()->debug("Streamer shutted down for {}", sourceId);
                    removeSource(sourceId, finished);
                };

            StreamingHandler& streamer = handlers.streamer;
            auto dropboxFolderShuttedDown =
                [&streamer, sourceId, streamerShuttedDown] () {
                    Log()->debug("Dropbox folder shutted down for {}", sourceId);
                    Log()->debug("Shutting down streamer for {}", sourceId);
                    streamer.shutdown(streamerShuttedDown);
                };

            DropboxFolder& dropboxFolder = handlers.dropboxFolder;
            auto splitterShuttedDown =
                [&dropboxFolder, sourceId, dropboxFolderShuttedDown] () {
                    Log()->debug("Splitter shutted down for {}", sourceId);
                    Log()->debug("Shutting down dropbox folder for {}", sourceId);
                    dropboxFolder.shutdown(dropboxFolderShuttedDown);
                };

            handlers.splitter.shutdown(splitterShuttedDown);
        }
    } else {
        Log()->debug("No sources registered");
        _ioService->post(finished);
    }
}

void Controller::loadConfig(
    const Protocol::ClientConfig& config,
    const std::function<void ()>& finished)
{
    Log()->trace(">> Controller::loadConfig");

    if(!_config.empty()) {
        auto loadConfig =
            std::bind(&Controller::loadConfig, this, config, finished);
        reset(loadConfig);
    } else {
        _config.loadConfig(config);

        _dropbox.setToken(_config.dropboxToken());

        _config.enumSources(
        [this] (const SourceConfig& config) -> bool {
            startHandleSource(config);
            return true;
        });

        _ioService->post(finished);
    }
}

void Controller::updateConfig(
    const Protocol::ClientConfig& config,
    const std::function<void ()>& finished)
{
    Log()->trace(">> Controller::updateConfig");

    loadConfig(config, finished);
}

void Controller::streamRequested(
    const Protocol::RequestStream& request,
    const std::function<void ()>& streaming,
    const std::function<void ()>& streamingFailed)
{
    Log()->trace(">> Controller::streamRequested");

    auto it = _handlers.find(request.sourceid());

    assert(_handlers.end() != it);

    if(_handlers.end() != it) {
        SourceHandlers& handlers = it->second;
        handlers.streamer.stream(
            request.destination(),
            streaming,
            streamingFailed);
    }
}

void Controller::stopStream(const Protocol::StopStream& request)
{
    Log()->trace(">> Controller::stopStream");

    auto it = _handlers.find(request.sourceid());

    assert(_handlers.end() != it);

    if(_handlers.end() != it) {
        SourceHandlers& handlers = it->second;
        handlers.streamer.stopStream();
    }
}

void Controller::scheduleShrinkStorage()
{
    _shrinkTimer.expires_from_now(std::chrono::seconds(SHRINK_INTERVAL));
    _shrinkTimer.async_wait(
        [this] (const asio::error_code& error) {
            if(error)
                return;

            shrinkStorage();
        }
    );
}

void Controller::shrinkStorage()
{
    for(auto& pair: _handlers) {
        SourceHandlers& handlers = pair.second;
        const SourceConfig& config = handlers.splitter.config();
        if(config.dropboxMaxStorage > 0)
            handlers.dropboxFolder.shrinkFolder(config.dropboxMaxStorage);
    }

    scheduleShrinkStorage();
}

void Controller::reset(const std::function<void ()>& finished)
{
    Log()->trace(">> Controller::reset");

    auto clearConfig =
        [this, finished] () {
            assert(_handlers.empty());
            _config.clear();
            _ioService->post(finished);
        };

    auto resetDropbox =
        [this, clearConfig] () {
            _dropbox.reset(clearConfig);
        };

    stopHandleSources(resetDropbox);
}

void Controller::shutdown(const std::function<void ()>& finished)
{
    Log()->trace(">> Controller::shutdown");

    auto dropboxShutdowned =
        [this, finished] () {
            _ioService->post(finished);
            _working.reset();
        };

    auto shutdownDropbox =
        [this, dropboxShutdowned] () {
            _dropbox.shutdown(dropboxShutdowned);
        };

    stopHandleSources(shutdownDropbox);
}

}
