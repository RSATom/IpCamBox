#pragma once

#include <memory>

#undef CHAR_WIDTH
#define SPDLOG_ENABLE_SYSLOG 1
#include <spdlog/spdlog.h>


void InitDaemonLoggers(const char* ident);

const std::shared_ptr<spdlog::logger>& DaemonLog();
