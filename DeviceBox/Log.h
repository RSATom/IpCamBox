#pragma once

#include <memory>

#undef CHAR_WIDTH
#define SPDLOG_ENABLE_SYSLOG 1
#include <spdlog/spdlog.h>


namespace DeviceBox
{

void InitDeviceBoxLoggers(bool daemon);

const std::shared_ptr<spdlog::logger>& Log();
const std::shared_ptr<spdlog::logger>& ClientLog();
const std::shared_ptr<spdlog::logger>& ControllerLog();
const std::shared_ptr<spdlog::logger>& DropboxLog();
const std::shared_ptr<spdlog::logger>& StreamingLog();
const std::shared_ptr<spdlog::logger>& SplittingLog();

}
