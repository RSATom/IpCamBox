#pragma once

#include <memory>

#include <asio.hpp>

#include "Common/RefCounter.h"

#include "SourceConfig.h"


namespace DeviceBox
{

class SplitHandler
{
public:
    SplitHandler(asio::io_service* io_service, const SourceConfig&);
    ~SplitHandler();

    const SourceConfig& config() const;

    bool active() const;

    void startSplit(
        const std::function<void (
            const std::string& dir,
            const std::string& name)>& fileReady);

    void shutdown(const std::function<void ()>& finished);

private:
    RefCounter<SplitHandler> _thisRefCounter;

    struct Private;
    std::unique_ptr<Private> _p;
};

}
