#include "Log.h"


static std::shared_ptr<spdlog::logger> ConfigLogger;

static void InitLoggers()
{
    spdlog::sink_ptr sink =
        std::make_shared<spdlog::sinks::stderr_sink_st>();

    ConfigLogger = spdlog::create("Config", { sink });

#ifndef NDEBUG
    ConfigLogger->set_level(spdlog::level::debug);
#else
    ConfigLogger->set_level(spdlog::level::info);
#endif
}

const std::shared_ptr<spdlog::logger>& ConfigLog()
{
    if(!ConfigLogger)
        InitLoggers();

    return ConfigLogger;
}
