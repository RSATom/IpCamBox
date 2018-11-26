#include "Log.h"


namespace ControlServer
{

static std::shared_ptr<spdlog::logger> ServerLogger;

void InitLoggers(bool daemon)
{
    spdlog::sink_ptr sink;
    if(daemon)
        sink = std::make_shared<spdlog::sinks::syslog_sink>("ControlServer", LOG_PID);
     else
        sink = std::make_shared<spdlog::sinks::stderr_sink_st>();

    ServerLogger = spdlog::create("ControlServer", { sink });

#ifndef NDEBUG
    ServerLogger->set_level(spdlog::level::debug);
#else
    ServerLogger->set_level(spdlog::level::info);
#endif
}

const std::shared_ptr<spdlog::logger>& Log()
{
    return ServerLogger;
}

}
