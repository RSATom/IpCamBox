#include "Server.h"

#include <Common/Keys.h>

#include "ServerSession.h"

#include "Protocol/protocol.h"


namespace ControlServer
{

///////////////////////////////////////////////////////////////////////////////
ServerSecureContext::ServerSecureContext(const ::Server::Config::Config* config) :
    asio::ssl::context(asio::ssl::context::sslv23),
    _config(config), _valid(false)
{
    using namespace asio;

    error_code error;

    set_options(
        ssl::context::default_workarounds |
        ssl::context::single_dh_use,
        error);
    if(error) {
        Log()->critical("set_options failed: {}", error.message());
        return;
    }

    use_tmp_dh(const_buffer(TmpDH2048, strlen(TmpDH2048)), error);
    if(error) {
        Log()->critical("use_tmp_dh failed: {}", error.message());
        return;
    }

#if CONTROL_USE_TLS
    set_verify_mode(
        ssl::verify_peer |
        ssl::verify_fail_if_no_peer_cert |
        ssl::verify_client_once,
        error);
    if(error) {
        Log()->critical("set_verify_mode failed: {}", error.message());
        return;
    }

    if(!updateCertificate())
        return;

#else
    set_verify_mode(asio::ssl::verify_none, error);
    if(error) {
        Log()->critical("set_verify_mode failed: {}", error.message());
        return;
    }

#ifndef NDEBUG
    SSL_CTX_set_cipher_list(native_handle(), "aNULL");
#endif
#endif

    _valid = true;
}

const ::Server::Config::Config * ServerSecureContext::config()
{
    return _config;
}

bool ServerSecureContext::valid() const
{
    return _valid;
}

bool ServerSecureContext::updateCertificate()
{
    Log()->trace(">> ServerSecureContext::updateCertificate");

    using namespace asio;

    const std::string certificate = config()->certificate();

    error_code error;

    use_private_key(
        const_buffer(certificate.data(), certificate.size()),
        ssl::context::pem,
        error);
    if(error) {
        Log()->critical("use_private_key failed: {}", error.message());
        _valid = false;
        return false;
    }

    use_certificate_chain(
        const_buffer(certificate.data(), certificate.size()),
        error);
    if(error) {
        Log()->critical("use_certificate_chain failed: {}", error.message());
        _valid = false;
        return false;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////
const std::shared_ptr<spdlog::logger>& Server::Log()
{
    return ControlServer::Log();
}

Server::Server(
    asio::io_service* ioService,
    const ::Server::Config::Config* config) :
    ServerSecureContext(config),
    NetworkCore::Server(ioService, config->serverConfig()->controlServerPort),
    _updateCertificateTimer(*ioService)
{
    scheduleUpdateCertificate();
}

void Server::scheduleUpdateCertificate()
{
    Log()->info("Scheduling update certificate within {} days", UPDATE_CERTIFICATE_TIMEOUT);

    _updateCertificateTimer.expires_from_now(std::chrono::minutes(UPDATE_CERTIFICATE_TIMEOUT));
    _updateCertificateTimer.async_wait(
        [this] (const asio::error_code& error) {
            if(error)
                return;

            updateCertificate();

            scheduleUpdateCertificate();
        }
    );
}

void Server::onNewConnection(const std::shared_ptr<asio::ip::tcp::socket>& socket)
{
    if(!valid()) {
        Log()->critical("Can't accept incoming connectin in invalid state.");
        return;
    }

    Log()->trace(
        ">> Server::onNewConnection. ip: {}",
        socket->remote_endpoint().address().to_string());

    std::shared_ptr<ServerSession> session =
        std::make_shared<ServerSession>(
            ioService(), config(), &_sessions,
            socket, static_cast<ServerSession::SecureContext*>(this));
    session->handshake();
}

void Server::requestStream(
    const DeviceId& deviceId,
    const SourceId& sourceId,
    const StreamDst& dst)
{
    SessionContext& sessionContext = _sessions.get(deviceId);

    sessionContext.streamRequested(sourceId, dst);

    if(ServerSession* session = sessionContext.activeSession()) {
        Log()->debug(
            "Requesting stream. deviceId: {}, sourceId: {}, streamDst: {}",
            deviceId, sourceId, dst);

        session->requestStream(sourceId);
    } else
        Log()->debug(
            "Requested stream for not connected device {}, sourceId: {}",
            deviceId, sourceId);
}

void Server::stopStream(
    const DeviceId& deviceId,
    const SourceId& sourceId)
{
    SessionContext& sessionContext = _sessions.get(deviceId);

    sessionContext.stopStreamRequested(sourceId);

    if(ServerSession* session = sessionContext.activeSession()) {
        Log()->debug(
            "Requesting stream stop. deviceId: {}, sourceId: {}",
            deviceId, sourceId);

        session->stopStream(sourceId);
    } else
        Log()->debug(
            "Requested stream stop for not connected device {}, sourceId: {}",
            deviceId, sourceId);
}

}
