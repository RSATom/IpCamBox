#include "Client.h"

#include <chrono>

#include <Common/Config.h>
#include <Common/Keys.h>
#include <Common/Hash.h>

#include "Protocol/protocol.h"

#include "Controller.h"


namespace DeviceBox
{

///////////////////////////////////////////////////////////////////////////////
ClientSecureContext::ClientSecureContext(const AuthConfig* authConfig) :
    asio::ssl::context(asio::ssl::context::sslv23),
    _valid(false),
    _authConfig(authConfig)
{
    asio::error_code error;

    set_options(
        asio::ssl::context::default_workarounds | asio::ssl::context::single_dh_use,
        error);
    if(error) {
        ClientLog()->critical("set_options failed: {}", error.message());
        return;
    }

    use_tmp_dh(asio::const_buffer(TmpDH2048, strlen(TmpDH2048)), error);
    if(error) {
        ClientLog()->critical("use_tmp_dh failed: {}", error.message());
        return;
    }

#if CONTROL_USE_TLS
#if DISABLE_VERIFY_CONTROL_SERVER
    set_verify_mode(asio::ssl::verify_none, error);
#else
    set_verify_mode(asio::ssl::verify_peer, error);
#endif
    if(error) {
        ClientLog()->critical("set_verify_mode failed: {}", error.message());
        return;
    }

    set_default_verify_paths(error);
    if(error) {
        ClientLog()->critical("set_default_verify_paths failed: {}", error.message());
        return;
    }

    use_certificate(
        asio::const_buffer(authConfig->certificate.data(), authConfig->certificate.size()),
        asio::ssl::context::pem,
        error);
    if(error) {
        ClientLog()->critical("Failed to load certificate: {}", error.message());
        return;
    }

    use_private_key(
        asio::const_buffer(authConfig->certificate.data(), authConfig->certificate.size()),
        asio::ssl::context::pem,
        error);
    if(error) {
        ClientLog()->critical("Failed to load key: {}", error.message());
        return;
    }
#else
    set_verify_mode(asio::ssl::verify_none, error);
    if(error) {
        ClientLog()->critical("set_verify_mode failed: {}", error.message());
        return;
    }

#ifndef NDEBUG
    SSL_CTX_set_cipher_list(native_handle(), "aNULL");
#endif
#endif

    _valid = true;
}

const AuthConfig* ClientSecureContext::authConfig() const
{
    return _authConfig;
}

bool ClientSecureContext::valid() const
{
    return _valid;
}

///////////////////////////////////////////////////////////////////////////////
const std::shared_ptr<spdlog::logger>& Client::Log()
{
    return ClientLog();
}

Client::Client(
    asio::io_service* ioService,
    Controller* controller) :
    ClientSecureContext(controller->authConfig()),
    NetworkCore::Client(ioService, static_cast<SecureContext*>(this)),
    _controller(controller),
    _port(0),
    _reconnectTimer(*ioService)
{
}

void Client::onError(const asio::error_code& error)
{
    std::string message = error.message();

    Log()->error(message);

    const std::function<void()> scheduleConnect =
        std::bind(&Client::scheduleConnect, this);

    const std::function<void()> resetController =
        std::bind(&Controller::reset, _controller, scheduleConnect);

    ioService().post(resetController);
}

void Client::onWriteFail(MessageType messageType, const std::string& message, const asio::error_code& errorCode)
{
    NetworkCore::Client::onWriteFail(messageType, message, errorCode);

    onError(errorCode);
}

void Client::connect(const std::string& server, unsigned short port)
{
    _server = server;
    _port = port;

    connect();
}

void Client::connect()
{
    if(!valid()) {
        Log()->critical("Connect cancelled");
        return;
    }

    if(shuttingDown()) {
        Log()->info("Connect cancelled due to shutting down");
        return;
    }

    Log()->info("Connecting to {}:{}", _server, _port);
    NetworkCore::Client::connect(_server, _port);
}

void Client::scheduleConnect()
{
    if(shuttingDown()) {
        Log()->info("Schedule connect cancelled due to shutting down");
        return;
    }

    Log()->info("Scheduling reconnect within {} seconds", RECONNECT_TIMEOUT);

    auto self = shared_from_this();
    _reconnectTimer.expires_from_now(std::chrono::seconds(RECONNECT_TIMEOUT));
    _reconnectTimer.async_wait(
        [self, this] (const asio::error_code& error) {
            if(error)
                return;

            connect();
        }
    );
}

void Client::onConnected(const asio::error_code& errorCode)
{
    NetworkCore::Client::onConnected(errorCode);

    if(errorCode) {
        onError(errorCode);
        return;
    }

    if(onConnected())
        readMessageAsync();
}

void Client::onMessage(
    MessageType type,
    const std::string& body,
    const asio::error_code& errorCode)
{
    if(errorCode) {
        onError(errorCode);
        return;
    }

    if(parseMessage(type, body))
        readMessageAsync();
}

template<typename MessageType>
bool Client::parseMessage(const std::string& body)
{
    MessageType message;
    if(message.ParseFromString(body))
        return onMessage(message);

    assert(false);

    return false;
}

bool Client::parseMessage(MessageType type, const std::string& body)
{
    switch(type) {
        case Protocol::ServerGreetingMessage:
            return parseMessage<Protocol::ServerGreeting>(body);
        case Protocol::ClientConfigReplyMessage:
            return parseMessage<Protocol::ClientConfigReply>(body);
        case Protocol::ClientConfigUpdatedMessage:
            return parseMessage<Protocol::ClientConfigUpdated>(body);
        case Protocol::RequestStreamMessage:
            return parseMessage<Protocol::RequestStream>(body);
        case Protocol::StopStreamMessage:
            return parseMessage<Protocol::StopStream>(body);
        default:
            assert(false);
            return false; // unknown message
    }
}

void Client::sendMessage(Protocol::MessageType messageType, const Message& message)
{
    std::string messageBody = message.SerializeAsString();
    writeMessageAsync(messageType, &messageBody);
}

bool Client::onConnected()
{
    Log()->info("Connected");

    Protocol::ClientGreeting message;

    sendMessage(Protocol::ClientGreetingMessage, message);

    return true;
}

bool Client::onMessage(const Protocol::ServerGreeting& message)
{
    Log()->debug("Got ServerGreeting");

    Protocol::ClientConfigRequest request;
    sendMessage(Protocol::ClientConfigRequestMessage, request);

    return true;
}

void Client::sendReady()
{
    Protocol::ClientReady message;
    sendMessage(Protocol::ClientReadyMessage, message);
}

bool Client::onMessage(const Protocol::ClientConfigReply& message)
{
    Log()->debug("Got ClientConfigReply");

    const std::function<void ()> ready =
        std::bind(
            &Client::sendReady,
            std::static_pointer_cast<Client>(shared_from_this()));

    ioService().post(
        std::bind(
            &Controller::loadConfig, _controller, message.config(),
            ready));

    return true;
}

bool Client::onMessage(const Protocol::ClientConfigUpdated& message)
{
    Log()->debug("Got ClientConfigUpdated");

    const std::function<void ()> ready =
        std::bind(
            &Client::sendReady,
            std::static_pointer_cast<Client>(shared_from_this()));

    ioService().post(
        std::bind(
            &Controller::updateConfig, _controller, message.config(),
            ready));

    return true;
}

void Client::sendStreamStatus(const std::string& sourceId, bool success)
{
    Log()->trace(">> Client::sendStreamStatus. sourceId: {}, success: {}", sourceId, success);

    Protocol::StreamStatus message;
    message.set_sourceid(sourceId);
    message.set_success(success);

    sendMessage(Protocol::StreamStatusMessage, message);
}

bool Client::onMessage(const Protocol::RequestStream& message)
{
    Log()->debug("Got RequestStream");

    const std::function<void ()> streaming =
        std::bind(
            &Client::sendStreamStatus,
            std::static_pointer_cast<Client>(shared_from_this()),
            message.sourceid(), true);
    const std::function<void ()> streamingFailed =
        std::bind(
            &Client::sendStreamStatus,
            std::static_pointer_cast<Client>(shared_from_this()),
            message.sourceid(), false);
    ioService().post(
        std::bind(
            &Controller::streamRequested, _controller, message,
            streaming,
            streamingFailed));

    return true;
}

bool Client::onMessage(const Protocol::StopStream& message)
{
    Log()->debug("Got StopStream");

    ioService().post(std::bind(&Controller::stopStream, _controller, message));
    return true;
}

void Client::shutdown(const std::function<void ()>& finished)
{
    _reconnectTimer.cancel();
    NetworkCore::Client::shutdown(finished);
}

}
