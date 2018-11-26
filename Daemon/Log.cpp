#include "Log.h"


static std::shared_ptr<spdlog::logger> DaemonLogger;

void InitDaemonLoggers(const char* ident)
{
    DaemonLogger = spdlog::syslog_logger("syslog", ident, LOG_PID);
}

const std::shared_ptr<spdlog::logger>& DaemonLog()
{
    return DaemonLogger;
}
