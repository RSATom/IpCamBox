#include <thread>

#include <unistd.h>

#include "Common/Config.h"
#include "Daemon/Daemon.h"
#include "Server/ServerMain.h"

#if USE_PG_CONFIG
    #include "Server/PGConfig/Config.h"
#else
    #include "Server/Config/MemoryConfig.h"
#endif


int main(int argc, char *argv[])
{
    bool runAsDaemon = false;

    const char* options = "d";

    int option;
    while((option = getopt(argc, argv, options)) != -1) {
        switch(option) {
            case 'd':
                runAsDaemon = true;
                break;
        }
    }

    if(argc != optind)
        return -1;

    asio::io_service ioService;

#if USE_PG_CONFIG
    Server::PGConfig::Config config;
    if(!config.serverConfig())
        return -1;
#else
    Server::MemoryConfig::Config config;
#endif

    if(runAsDaemon) {
        if(Daemonize(&ioService))
            ServerMain(&ioService, &config, true);
    } else
        ServerMain(&ioService, &config, false);

    return 0;
}
