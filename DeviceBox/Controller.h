#pragma once

#include <asio.hpp>

#include "Common/CommonTypes.h"

#include "Protocol/protocol.h"

#include "Config.h"
class SourceConfig; // #include "SourceConfig.h"

#include "Log.h"
#include "AuthConfig.h"
#include "StreamingHandler.h"
#include "SplitHandler.h"
#include "Dropbox.h"
#include "DropboxFolder.h"


namespace DeviceBox
{

class Controller
{
public:
    Controller(asio::io_service*, const AuthConfig&);
    ~Controller();

    const AuthConfig* authConfig() const;

    void loadConfig(const Protocol::ClientConfig&,
                    const std::function<void ()>& finished);
    void updateConfig(const Protocol::ClientConfig&,
                    const std::function<void ()>& finished);
    void streamRequested(const Protocol::RequestStream&,
                         const std::function<void ()>& streaming,
                         const std::function<void ()>& streamingFailed);
    void stopStream(const Protocol::StopStream&);

    void reset(const std::function<void ()>& finished);
    void shutdown(const std::function<void ()>& finished);

private:
    enum {
        SHRINK_INTERVAL = 10,
    };

    static inline const std::shared_ptr<spdlog::logger>& Log();

    void startHandleSource(const SourceConfig& config);
    void stopHandleSources(const std::function<void ()>& finished);

    void startSplit(const SourceConfig& config);
    void newFileAvailable(const SplitHandler*, const std::string& dir, const std::string& name);

    void scheduleShrinkStorage();
    void shrinkStorage();

    void removeSource(const SourceId& source, const std::function<void ()>& finished);

private:
    asio::io_service* _ioService;
    std::unique_ptr<asio::io_service::work> _working;

    const AuthConfig _authConfig;

    Config _config;

    Dropbox _dropbox;

    struct SourceHandlers
    {
        SourceHandlers(
            asio::io_service* ioService,
            const SourceConfig& config,
            Dropbox* dropbox,
            const AuthConfig*);

        SplitHandler splitter;
        StreamingHandler streamer;
        DropboxFolder dropboxFolder;
    };

    std::map<SourceId, SourceHandlers> _handlers;

    asio::steady_timer _shrinkTimer;
};

}
