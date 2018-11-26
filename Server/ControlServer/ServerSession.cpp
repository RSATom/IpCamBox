#include "ServerSession.h"

#include <Common/Hash.h>

#include "Sessions.h"


namespace ControlServer
{

const std::shared_ptr<spdlog::logger>& ServerSession::Log()
{
    return ControlServer::Log();
}

ServerSession::ServerSession(
    asio::io_service* ioService,
    const ::Server::Config::Config* config,
    Sessions* sessions,
    const std::shared_ptr<asio::ip::tcp::socket>& socket,
    SecureContext* context) :
    NetworkCore::ServerSession(socket, context),
    _ioService(ioService),
    _requestStreamTimer(*ioService),
    _config(config),
    _sessions(sessions),
    _clientIp(socket->remote_endpoint().address()),
    _sessionContext(nullptr)
{
    Log()->info("Session created. Client ip: {}", _clientIp.to_string());

    asio::error_code error;
    secureStream().set_verify_callback(
        std::bind(
            &ServerSession::verifyClient, this,
            std::placeholders::_1, std::placeholders::_2),
        error);
    if(error) {
        Log()->critical("set_verify_callback failed: {}", error.message());
        return;
    }
}

ServerSession::~ServerSession()
{
    assert(_deviceId.empty() == (_sessionContext == nullptr));

    Log()->info(
        "Session destroying. Client ip: {}, deviceId: {}",
        _clientIp.to_string(),
        _deviceId);

    if(!_deviceId.empty())
        _sessionContext->destroyed(_deviceId);
}

bool ServerSession::verifyClient(
    bool /*preverified*/,
    asio::ssl::verify_context& context)
{
    X509* clientCert =
        X509_STORE_CTX_get_current_cert(context.native_handle());
    if(!clientCert) {
        Log()->error("X509_STORE_CTX_get_current_cert failed");
        return false;
    }

    std::string name;
    if(!_config->authenticate(clientCert, &name))
        return false;

    if(name.empty()) {
        Log()->error("Empty device Id");
        return false;
    }

    _deviceId = name;

    return true;
}

void ServerSession::onConnected(const asio::error_code& errorCode)
{
    NetworkCore::ServerSession::onConnected(errorCode);

    if(errorCode)
        return;

    Log()->info(
        "Secure channel established. Client ip: {}, DeviceId: {}",
        _clientIp.to_string(), _deviceId);

    ::Server::Config::Device device;
    if(!_config->findDevice(_deviceId, &device)) {
        Log()->error("Unknown device. Device: {}", _deviceId);
        return;
    }

    SessionContext& sessionContext = _sessions->get(_deviceId);
    if(sessionContext.activeSession()) {
        Log()->error("Device already connected. Device: {}", _deviceId);
        return;
    }

    _device = device;
    _sessionContext = &sessionContext;
    _sessionContext->authenticated(_deviceId, this);

    readMessageAsync();
}

void ServerSession::onWriteFail(MessageType messageType, const std::string& message, const asio::error_code& errorCode)
{
    NetworkCore::ServerSession::onWriteFail(messageType, message, errorCode);

    Log()->error(errorCode.message());
}

void ServerSession::onMessage(
    MessageType type, const std::string& body,
    const asio::error_code& errorCode)
{
    NetworkCore::ServerSession::onConnected(errorCode);

    if(errorCode) {
        Log()->error(errorCode.message());
        return;
    }

    if(parseMessage(type, body))
        readMessageAsync();
}

template<typename MessageType>
bool ServerSession::parseMessage(const std::string& body)
{
    MessageType message;
    if(message.ParseFromString(body))
        return onMessage(message);

    assert(false);

    return false;
}

bool ServerSession::parseMessage(MessageType type, const std::string& body)
{
    switch(type) {
        case Protocol::ClientGreetingMessage:
            return parseMessage<Protocol::ClientGreeting>(body);
        case Protocol::ClientConfigRequestMessage:
            return parseMessage<Protocol::ClientConfigRequest>(body);
        case Protocol::ClientReadyMessage:
            return parseMessage<Protocol::ClientReady>(body);
        case Protocol::StreamStatusMessage:
            return parseMessage<Protocol::StreamStatus>(body);
        default:
            assert(false);
            return false; // unknown message
    }
}

void ServerSession::sendMessage(Protocol::MessageType messageType, const Message& message)
{
    std::string messageBody = message.SerializeAsString();
    writeMessageAsync(messageType, &messageBody);
}

bool ServerSession::onMessage(const Protocol::ClientGreeting& message)
{
    Log()->debug("Got ClientGreeting");

    Protocol::ServerGreeting reply;

    sendMessage(Protocol::ServerGreetingMessage, reply);

    return true;
}

bool ServerSession::onMessage(const Protocol::ClientConfigRequest& message)
{
    Log()->debug("Got ClientConfigRequest");

    if(_device.id.empty()) {
        Log()->error("Not authenticated");
        return false;
    }

    Protocol::ClientConfigReply reply;

    Protocol::ClientConfig& config = *(reply.mutable_config());

    Protocol::DropboxConfig& dropbox = *config.mutable_dropbox();
    dropbox.set_token(_device.dropboxToken);

    _config->enumDeviceSources(_device.id,
        [&config] (const ::Server::Config::Source& sourceConfig) -> bool {
            Protocol::VideoSource& source = *(config.add_sources());
            source.set_id(sourceConfig.id);
            source.set_uri(sourceConfig.uri);
            source.set_dropboxmaxstorage(sourceConfig.dropboxMaxStorage);
            return true;
        }
    );

    sendMessage(Protocol::ClientConfigReplyMessage, reply);

    return true;
}

bool ServerSession::onMessage(const Protocol::ClientReady&)
{
    Log()->debug("Got ClientReady");

    if(!_sessionContext) {
        Log()->error("No session context");
        return false;
    }

    _sessionContext->enumActiveStreams(
        [this] (const SourceId& sourceId, const StreamDst&) {
            Log()->debug(
                "Restoring stream for source \"{}\"",
                 sourceId);
            _ioService->post(std::bind(&ServerSession::requestStream, this, sourceId));
            return true;
        }
    );

    return true;
}

bool ServerSession::onMessage(const Protocol::StreamStatus& message)
{
    Log()->debug("Got StreamStatus");

    if(!_sessionContext) {
        Log()->error("No session context");
        return false;
    }

    if(message.success())
        Log()->debug("{} is streaming", message.sourceid());
    else {
        const SourceId sourceId = message.sourceid();
        Log()->debug("{} is NOT streaming", message.sourceid());

        if(_sessionContext->shouldStream(message.sourceid())) {
            Log()->debug("Schedule {} streaming", message.sourceid());

            _requestStreamTimer.expires_from_now(std::chrono::seconds(10));
            _requestStreamTimer.async_wait(
                [this, sourceId] (const asio::error_code& error) {
                    if(error)
                        return;

                    requestStream(sourceId);
                }
            );
        }
    }

    return true;
}

void ServerSession::requestStream(const SourceId& sourceId)
{
    StreamDst dst;
    if(_sessionContext->shouldStream(sourceId, &dst)) {
        Protocol::RequestStream requestStream;
        requestStream.set_sourceid(sourceId);
        requestStream.set_destination(dst);

        Log()->debug(
            "Requesting stream from {} to {}",
            requestStream.sourceid(),
            requestStream.destination());

        sendMessage(Protocol::RequestStreamMessage, requestStream);
    }
}

void ServerSession::stopStream(const SourceId& sourceId)
{
    Protocol::StopStream stopStream;
    stopStream.set_sourceid(sourceId);

    sendMessage(Protocol::StopStreamMessage, stopStream);
}

}
