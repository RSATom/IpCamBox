#pragma once

#include <asio.hpp>


// return true for daemonized process
bool Daemonize(asio::io_context*);
