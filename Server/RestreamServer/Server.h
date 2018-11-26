#pragma once

#include <functional>

#include <glib.h>

#include <gst/rtsp/gstrtspurl.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <asio.hpp>

#include <RtspRestreamServer/RestreamServerLib/Action.h>

#include <Common/CommonTypes.h>

#include "Log.h"
#include "Config/Config.h"


namespace RestreamServer
{

class Server
{
public:
    typedef void (SourceCallback) (const std::string& deviceName, const std::string& sourceName);

    Server(
        asio::io_service*,
        const ::Server::Config::Config*);

    ~Server();

    const ::Server::Config::Config* config() const;

    void runServer(
        const std::function<SourceCallback>& firstReaderConnectedCallback,
        const std::function<SourceCallback>& lastReaderDisconnectedCallback);

private:
    struct ClientInfo;
    struct FactoryInfo;

    static inline const std::shared_ptr<spdlog::logger>& Log();

    void serverMain();

    bool updateCertificate();
    void scheduleUpdateCertificate();

    bool tlsAuthenticate(GTlsCertificate*, UserName*);
    bool authenticationRequired(GstRTSPMethod method, const std::string& path);
    bool authenticate(const std::string& userName, const std::string& pass);

    bool authorize(
        const UserName& userName,
        RestreamServerLib::Action,
        const std::string& path,
        bool record);

    std::string extractSourceId(const std::string& pass) const;

    void firstPlayerConnected(const std::string& path);
    void lastPlayerDisconnected(const std::string& path);
    void recorderConnected(const std::string& path);
    void recorderDisconnected(const std::string& path);

private:
    struct Private;
    std::unique_ptr<Private> _p;
};

}
