#include "Log.h"


namespace DeviceBox
{

static std::shared_ptr<spdlog::logger> GenericLogger;
static std::shared_ptr<spdlog::logger> ClientLogger;
static std::shared_ptr<spdlog::logger> ControllerLogger;
static std::shared_ptr<spdlog::logger> DropboxLogger;
static std::shared_ptr<spdlog::logger> StreamerLogger;
static std::shared_ptr<spdlog::logger> SplitterLogger;

void InitDeviceBoxLoggers(bool daemon)
{
    if(GenericLogger)
        return;

    spdlog::sink_ptr sink;
    if(daemon)
        sink = std::make_shared<spdlog::sinks::syslog_sink>("DeviceBox", LOG_PID);
     else
        sink = std::make_shared<spdlog::sinks::stderr_sink_st>();

    GenericLogger = spdlog::create("DeviceBox", { sink });
    ClientLogger = spdlog::create("DeviceBox Client", { sink });
    ControllerLogger = spdlog::create("DeviceBox Controller", { sink });
    DropboxLogger = spdlog::create("DeviceBox Dropbox", { sink });
    StreamerLogger = spdlog::create("DeviceBox Streamer", { sink });
    SplitterLogger = spdlog::create("DeviceBox Splitter", { sink });

#ifndef NDEBUG
    GenericLogger->set_level(spdlog::level::debug);
    ClientLogger->set_level(spdlog::level::debug);
    ControllerLogger->set_level(spdlog::level::debug);
    DropboxLogger->set_level(spdlog::level::debug);
    StreamerLogger->set_level(spdlog::level::debug);
    SplitterLogger->set_level(spdlog::level::debug);
#else
    GenericLogger->set_level(spdlog::level::info);
    ClientLogger->set_level(spdlog::level::info);
    ControllerLogger->set_level(spdlog::level::info);
    DropboxLogger->set_level(spdlog::level::info);
    StreamerLogger->set_level(spdlog::level::info);
    SplitterLogger->set_level(spdlog::level::info);
#endif
}

const std::shared_ptr<spdlog::logger>& Log()
{
    return GenericLogger;
}

const std::shared_ptr<spdlog::logger>& ClientLog()
{
    return ClientLogger;
}

const std::shared_ptr<spdlog::logger>& ControllerLog()
{
    return ControllerLogger;
}

const std::shared_ptr<spdlog::logger>& DropboxLog()
{
    return DropboxLogger;
}

const std::shared_ptr<spdlog::logger>& StreamingLog()
{
    return StreamerLogger;
}

const std::shared_ptr<spdlog::logger>& SplittingLog()
{
    return SplitterLogger;
}

}
