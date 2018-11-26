#pragma once

#include <string>

#include <asio.hpp>

#include "AuthConfig.h"


void DeviceBoxMain(
    asio::io_service* ioService,
    const DeviceBox::AuthConfig& authConfig,
    const std::string& server,
    unsigned short port,
    bool daemon);
