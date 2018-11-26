#pragma once

#include "NetworkCore/session.h"
#include "Protocol/protocol.h"

#include <Common/CommonTypes.h>

#include "Log.h"
#include "Config/Config.h"


namespace ControlServer
{

class Sessions;
class SessionContext;

class ServerSession : public NetworkCore::ServerSession
{
public:
    ServerSession(
        asio::io_service* ioService,
        const ::Server::Config::Config* config,
        Sessions*,
        const std::shared_ptr<asio::ip::tcp::socket>& socket,
        SecureContext*);
    ~ServerSession();

    void onMessage(MessageType, const std::string&, const asio::error_code&) override;
    void onWriteFail(MessageType, const std::string&, const asio::error_code&) override;

    void requestStream(const SourceId&);
    void stopStream(const SourceId&);

private:
    static inline const std::shared_ptr<spdlog::logger>& Log();

    bool verifyClient(
        bool preverified,
        asio::ssl::verify_context&);
    void onConnected(const asio::error_code& errorCode) override;

    typedef google::protobuf::MessageLite Message;
    void sendMessage(Protocol::MessageType, const Message&);

    template<typename MessageType>
    bool parseMessage(const std::string& body);
    bool parseMessage(MessageType type, const std::string& body);

    bool onMessage(const Protocol::ClientGreeting&);
    bool onMessage(const Protocol::ClientConfigRequest&);
    bool onMessage(const Protocol::ClientReady&);
    bool onMessage(const Protocol::StreamStatus&);

private:
    asio::io_service* _ioService;
    asio::steady_timer _requestStreamTimer;

    const ::Server::Config::Config *const _config;

    Sessions* _sessions;
    asio::ip::address _clientIp;

    DeviceId _deviceId;
    ::Server::Config::Device _device;
    SessionContext *_sessionContext;

    std::string _nonce;
};

}
