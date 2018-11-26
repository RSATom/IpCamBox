#pragma once

#include <asio.hpp>

#include <Common/CommonTypes.h>

#include "Config/Config.h"


void ServerMain(
    asio::io_service* ioService,
    const Server::Config::Config* config,
    bool daemon);
