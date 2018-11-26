#include <thread>

#include "Common/Config.h"
#include "NetworkCore/Log.h"
#include "Server/Config/MemoryConfig.h"
#include "Server/PGConfig/Config.h"
#include "Server/ServerMain.h"
#include "DeviceBox/DeviceBoxMain.h"

#if !USE_PG_CONFIG
#include "Common/Keys.h"
#endif

int main(int argc, char *argv[])
{
    // to avoid race conditions
    NetworkCore::InitLoggers(false);

#if USE_PG_CONFIG
    const std::string deviceId = "f749314e-2544-11e8-b7dc-57718091ce2f";

    Server::PGConfig::Config config;

    /*
    {
    const Server::Config::Server* server = config.serverConfig();

    const bool deviceFound = config.findDevice("f749314e-2544-11e8-b7dc-57718091ce2f");
    Server::Config::Device device;
    const bool deviceFound2 = config.findDevice("f749314e-2544-11e8-b7dc-57718091ce2f", &device);

    const bool deviceSourceFound =
        config.findDeviceSource(
            "f749314e-2544-11e8-b7dc-57718091ce2f",
            "9aab99e8-25e6-11e8-b2bc-2790a1db18e7"
            );

    Server::Config::Source source;
    const bool deviceSourceFound2 =
        config.findDeviceSource(
            "f749314e-2544-11e8-b7dc-57718091ce2f",
            "9aab99e8-25e6-11e8-b2bc-2790a1db18e7",
            &source
            );

    config.enumDeviceSources(
        "f749314e-2544-11e8-b7dc-57718091ce2f",
        [] (const Server::Config::Source& source) {
            return true;
        }
    );

    const bool userFound = config.findUser(UserName());
    Server::Config::User user;
    const bool userFound2 = config.findUser(UserName(), &user);

    const bool userSourceFound =
        config.findUserSource(
            UserName(),
            "9aab99e8-25e6-11e8-b2bc-2790a1db18e7");
    Server::Config::PlaySource playSource;
    const bool userSourceFound2 =
        config.findUserSource(
            UserName(),
            "9aab99e8-25e6-11e8-b2bc-2790a1db18e7",
            &playSource);
    }
    */

    Server::Config::Device device;
    if(!config.findDevice(deviceId, &device))
        return -1;

    DeviceBox::AuthConfig authConfig {
        .certificate = device.certificate,
    };
#else
    Server::MemoryConfig::Config config;

    DeviceBox::AuthConfig authConfig {
        .certificate = std::string(TestClientCertificate) + TestClientKey,
    };
#endif

    const Server::Config::Server* server = config.serverConfig();
    if(!server)
        return -1;

    std::thread serverThread(
        [] () {
#if USE_PG_CONFIG
            Server::PGConfig::Config config;
#else
            Server::MemoryConfig::Config config;
#endif
            asio::io_service ioService;
            ServerMain(&ioService, &config, false);
        }
    );

    std::this_thread::sleep_for(std::chrono::seconds(1));

    asio::io_service ioService;
    DeviceBoxMain(
        &ioService, authConfig,
        server->serverHost,
        server->controlServerPort,
        false);

    serverThread.join();

    return 0;
}
