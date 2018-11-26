#include "ServerMain.h"

#include <asio.hpp>

#include <Common/Config.h>

#include "Log.h"

#include "ControlServer/Server.h"
#include "RestreamServer/Server.h"


void ServerMain(
    asio::io_service* ioService,
    const Server::Config::Config* config,
    bool daemon)
{
    InitServerLoggers(daemon);

    ControlServer::Server server(ioService, config);
    RestreamServer::Server restreamServer(ioService, config);

    const std::string restreamServerUrl =
        fmt::format(
#if RESTREAMER_USE_TLS
            "rtsps://{}:{}/",
#else
            "rtsp://{}:{}/",
#endif
            config->serverConfig()->serverHost,
            config->serverConfig()->restreamServerPort);
    auto firstReaderConnected =
        [&restreamServerUrl, &server]
        (const std::string& deviceName, const std::string& sourceName) {
            server.requestStream(
                deviceName, sourceName,
                restreamServerUrl + sourceName);
        };
    auto lastReaderDisconnected =
        [&server] (const std::string& deviceName, const std::string& sourceName) {
            server.stopStream(deviceName, sourceName);
        };

    server.startAccept();
    restreamServer.runServer(firstReaderConnected, lastReaderDisconnected);

    /*
    asio::steady_timer testTimer(*ioService);
    testTimer.expires_from_now(std::chrono::seconds(10));
    testTimer.async_wait(
        [&rtspServer] (const asio::error_code& error) {
            rtspServer.testPost();
        }
    );
    */

    ioService->run();
}
