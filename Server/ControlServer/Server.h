#pragma once

#include <asio/ssl.hpp>

#include <CxxPtr/OpenSSLPtr.h>

#include <Common/CommonTypes.h>

#include "Log.h"
#include "NetworkCore/server.h"
#include "Config/Config.h"
#include "Sessions.h"


namespace ControlServer
{

///////////////////////////////////////////////////////////////////////////////
class ServerSecureContext : public asio::ssl::context
{
public:
    ServerSecureContext(const ::Server::Config::Config*);

protected:
    const ::Server::Config::Config * config();

    bool valid() const;

    bool updateCertificate();

private:
    const ::Server::Config::Config *const _config;

    bool _valid;
};


///////////////////////////////////////////////////////////////////////////////
class Server : private ServerSecureContext, public NetworkCore::Server
{
public:
    Server(
        asio::io_service* ioService,
        const ::Server::Config::Config*);

    void requestStream(const DeviceId&, const SourceId&, const StreamDst&);
    void stopStream(const DeviceId&, const SourceId&);

protected:
    void onNewConnection(const std::shared_ptr<asio::ip::tcp::socket>& socket) override;

private:
    static inline const std::shared_ptr<spdlog::logger>& Log();

    void scheduleUpdateCertificate();

private:
    asio::steady_timer _updateCertificateTimer;
    Sessions _sessions;
};

}
