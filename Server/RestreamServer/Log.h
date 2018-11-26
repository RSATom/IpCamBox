#pragma once

#include <memory>

#ifdef CHAR_WIDTH
#define CHAR_WIDTH_SAVE CHAR_WIDTH
#undef CHAR_WIDTH
#endif
#define SPDLOG_ENABLE_SYSLOG 1
#include <spdlog/spdlog.h>
#ifdef CHAR_WIDTH_SAVE
#define CHAR_WIDTH CHAR_WIDTH_SAVE
#undef CHAR_WIDTH_SAVE
#endif


namespace RestreamServer
{

void InitLoggers(bool daemon);

const std::shared_ptr<spdlog::logger>& Log();

}
