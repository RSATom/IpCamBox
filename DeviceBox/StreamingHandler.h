#pragma once

#include <memory>

#include <asio.hpp>

#include "Common/RefCounter.h"

#include "AuthConfig.h"
#include "SourceConfig.h"


namespace DeviceBox
{

class StreamingHandler
{
public:
    StreamingHandler(asio::io_service*, const SourceConfig&, const AuthConfig*);
    ~StreamingHandler();

    bool active() const;

    // streamFailed() could be called even if streaming() was called previously
    void stream(const std::string& destination,
                const std::function<void ()>& streaming,
                const std::function<void ()>& streamFailed);
    void stopStream();

    void shutdown(const std::function<void ()>& finished);

private:
    void streaming(const std::function<void ()>& outerStreaming);
    void streamFailed(const std::function<void ()>& outerStreamFailed);
    void destroyStreaming();

private:
    RefCounter<StreamingHandler> _thisRefCounter;

    struct Private;
    std::unique_ptr<Private> _p;
};

}
