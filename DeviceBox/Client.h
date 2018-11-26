#pragma once

#include "NetworkCore/client.h"

#include "Protocol/protocol.h"

#include "Log.h"
#include "AuthConfig.h"
#include "Controller.h"


namespace DeviceBox
{

///////////////////////////////////////////////////////////////////////////////
class ClientSecureContext : public asio::ssl::context
{
public:
    ClientSecureContext(const AuthConfig*);

protected:
    const AuthConfig* authConfig() const;
    bool valid() const;

private:
    bool _valid;
    const AuthConfig * const _authConfig;
};


///////////////////////////////////////////////////////////////////////////////
class Client : private ClientSecureContext, public NetworkCore::Client
{
public:
    enum {
#ifdef NDEBUG
        RECONNECT_TIMEOUT = 60, // seconds
#else
        RECONNECT_TIMEOUT = 5, // seconds
#endif
    };

    Client(asio::io_service* ioService, Controller*);

    void connect(const std::string& server, unsigned short port);

    typedef google::protobuf::MessageLite Message;

    void sendMessage(Protocol::MessageType, const Message&);

    void shutdown(const std::function<void ()>& finished) override;

protected:
    void onConnected(const asio::error_code& errorCode) override;
    void onMessage(MessageType, const std::string&, const asio::error_code&) override;
    void onWriteFail(MessageType, const std::string&, const asio::error_code&) override;

private:
    static inline const std::shared_ptr<spdlog::logger>& Log();

    void onError(const asio::error_code&);

    void connect();
    void scheduleConnect();

    bool parseMessage(MessageType type, const std::string& body);
    template<typename MessageType>
    bool parseMessage(const std::string& body);

    bool onConnected();
    bool onMessage(const Protocol::ServerGreeting&);
    void sendReady();
    bool onMessage(const Protocol::ClientConfigReply&);
    bool onMessage(const Protocol::ClientConfigUpdated&);
    void sendStreamStatus(const std::string& sourceId, bool success);
    bool onMessage(const Protocol::RequestStream&);
    bool onMessage(const Protocol::StopStream&);

private:
    Controller *const _controller;

    std::string _server;
    unsigned short _port;

    asio::steady_timer _reconnectTimer;
};

}
