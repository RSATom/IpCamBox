#include "Log.h"


namespace RestreamServer
{

static std::shared_ptr<spdlog::logger> RestreamServer;

void InitLoggers(bool daemon)
{
    spdlog::sink_ptr sink;
    if(daemon)
        sink = std::make_shared<spdlog::sinks::syslog_sink>("RestreamServer", LOG_PID);
     else
        sink = std::make_shared<spdlog::sinks::stderr_sink_st>();

    RestreamServer = spdlog::create("RestreamServer", { sink });

#ifndef NDEBUG
    RestreamServer->set_level(spdlog::level::debug);
#else
    RestreamServer->set_level(spdlog::level::info);
#endif
}

const std::shared_ptr<spdlog::logger>& Log()
{
    return RestreamServer;
}

}
