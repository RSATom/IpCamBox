#include "DeviceBoxMain.h"

#include <memory>

#include "NetworkCore/Log.h"

#include "Log.h"
#include "Controller.h"
#include "Client.h"


void DeviceBoxMain(
    asio::io_service* ioService,
    const DeviceBox::AuthConfig& authConfig,
    const std::string& server,
    unsigned short port,
    bool daemon)
{
    using namespace DeviceBox;

    NetworkCore::InitLoggers(daemon);
    InitDeviceBoxLoggers(daemon);

    Controller controller(ioService, authConfig);
    auto client = std::make_shared<Client>(ioService, &controller);
    client->connect(server, port);

    auto controllerShuttedDown =
        [&] () {
            ioService->stop();
        };

    auto clientShuttedDown =
        [&] () {
            controller.shutdown(controllerShuttedDown);
        };

    auto shutdown =
        [&] (const asio::error_code& error) {
            client->shutdown(clientShuttedDown);
        };

    asio::signal_set signals(*ioService, SIGINT, SIGTERM);
    signals.async_wait(
        [shutdown] (const asio::error_code& error, int signal_number) {
            shutdown(error);
        }
    );

#if !defined(NDEBUG) && 0
    asio::steady_timer testTimer(*ioService);
    testTimer.expires_from_now(std::chrono::seconds(10));
    testTimer.async_wait(shutdown);
#endif

    ioService->run();
}
