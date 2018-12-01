#include "Server.h"

#include <gst/gst.h>

#include <CxxPtr/GlibPtr.h>

#include <RtspRestreamServer/RestreamServerLib/Server.h>

#include "Log.h"


extern "C" {
GST_PLUGIN_STATIC_DECLARE(interpipe);
}

namespace RestreamServer
{

namespace {

struct PathInfo {
    DeviceId deviceId;
    SourceId sourceId;
    bool hasPlayers;
    bool hasRecorder;
};

}

struct Server::Private
{
    Private(
        asio::io_service*,
        const ::Server::Config::Config* config);

    asio::io_service* ioService;

    std::unique_ptr<const ::Server::Config::Config> config;

    asio::steady_timer updateCertificateTimer;

    std::function<Server::SourceCallback> firstReaderConnectedCallback;
    std::function<Server::SourceCallback> lastReaderDisconnectedCallback;

    std::thread serverThread;
    std::unique_ptr<RestreamServerLib::Server> restreamServer;

    std::map<std::string, PathInfo> pathsInfo;
};

Server::Private::Private(
    asio::io_service* ioService,
    const ::Server::Config::Config* config) :
    ioService(ioService), config(config->clone()),
    updateCertificateTimer(*ioService)
{
}


const std::shared_ptr<spdlog::logger>& Server::Log()
{
    return RestreamServer::Log();
}

Server::Server(
    asio::io_service* ioService,
    const ::Server::Config::Config* config) :
    _p(new Private(ioService, config))
{
    gst_init(0, nullptr);

    scheduleUpdateCertificate();
}

Server::~Server()
{
    _p.reset();
}

const ::Server::Config::Config* Server::config() const
{
    return _p->config.get();
}

void Server::runServer(
    const std::function<SourceCallback>& firstReaderConnectedCallback,
    const std::function<SourceCallback>& lastReaderDisconnectedCallback)
{
    if(_p->serverThread.get_id() != std::thread::id())
        return;

    _p->firstReaderConnectedCallback  = firstReaderConnectedCallback;
    _p->lastReaderDisconnectedCallback  = lastReaderDisconnectedCallback;

    _p->serverThread =
        std::thread(&Server::serverMain, this);
}

void Server::serverMain()
{
    using namespace RestreamServerLib;

    gst_init(0, nullptr);

    GST_PLUGIN_STATIC_REGISTER(interpipe);

    const ::Server::Config::Server& config = *_p->config->serverConfig();

    Callbacks callbacks;
    callbacks.tlsAuthenticate =
        std::bind(
            &Server::tlsAuthenticate,
            this,
            std::placeholders::_1,
            std::placeholders::_2);
    callbacks.authenticationRequired =
        std::bind(
            &Server::authenticationRequired,
            this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3);
    callbacks.authenticate =
        std::bind(
            &Server::authenticate,
            this,
            std::placeholders::_1,
            std::placeholders::_2);
    callbacks.authorize =
        std::bind(
            &Server::authorize,
            this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4);

    callbacks.firstPlayerConnected =
        std::bind(&Server::firstPlayerConnected, this, std::placeholders::_1, std::placeholders::_2);
    callbacks.lastPlayerDisconnected =
        std::bind(&Server::lastPlayerDisconnected, this, std::placeholders::_1);
    callbacks.recorderConnected =
        std::bind(&Server::recorderConnected, this, std::placeholders::_1, std::placeholders::_2);
    callbacks.recorderDisconnected =
        std::bind(&Server::recorderDisconnected, this, std::placeholders::_1);

#if RESTREAMER_USE_TLS
    const bool useTls = true;
#else
    const bool useTls = false;
#endif

    _p->restreamServer.reset(
        new RestreamServerLib::Server(
            callbacks,
            config.staticServerPort, config.restreamServerPort,
            useTls));

#if RESTREAMER_USE_TLS
    if(!updateCertificate())
        return;
#endif

    _p->restreamServer->serverMain();
}

bool Server::updateCertificate()
{
    Log()->trace(">> ServerSecureContext::updateCertificate");

    const std::string certificate = _p->config->certificate();

    if(certificate.empty()) {
        Log()->critical("Empty ceritficate");
        return false;
    }

    GError* certError = nullptr;

    GTlsCertificatePtr certificatePtr(
        g_tls_certificate_new_from_pem(
            certificate.data(), -1, &certError));

    if(certError) {
        Log()->critical("Failed to load certificate: {}", certError->message);
        g_error_free(certError);
        return false;
    }

    if(!certificatePtr)
        return false;

    _p->restreamServer->setTlsCertificate(certificatePtr.get());

    return true;
}

void Server::scheduleUpdateCertificate()
{
    Log()->info("Scheduling update certificate within {} days", UPDATE_CERTIFICATE_TIMEOUT);

    _p->updateCertificateTimer.expires_from_now(std::chrono::minutes(UPDATE_CERTIFICATE_TIMEOUT));
    _p->updateCertificateTimer.async_wait(
        [this] (const asio::error_code& error) {
            if(error)
                return;

            updateCertificate();

            scheduleUpdateCertificate();
        }
    );
}

bool Server::tlsAuthenticate(GTlsCertificate* cert, UserName* userName)
{
    Log()->trace(">> Server::authenticate. With certificate.");

    return _p->config->authenticate(cert, userName);
}

bool Server::authenticationRequired(
    GstRTSPMethod /*method*/,
    const std::string& path,
    bool record)
{
    Log()->trace(">> Server.authenticationRequired. url: {}", path);

    const SourceId sourceId = extractSourceId(path);
    if(sourceId.empty())
        return true;

    if(!record && _p->config->findUserSource(UserName(), sourceId)) {
        Log()->trace("SourceId \"{}\" DOES NOT require authentication as anonymous", sourceId);
        return false;
    }

    Log()->debug(
        "SourceId \"{}\" REQUIRE authentication for {}",
        sourceId,
        record ? "RECORD" : "PLAY");

    return true;
}

bool Server::authenticate(const std::string& userName, const std::string& pass)
{
    Log()->trace(">> Server.authenticate. user: {}, pass: {}", userName, pass);

    if(!_p->config)
        return false;

    std::string name(userName);

    ::Server::Config::User user;
    if(!_p->config->findUser(userName, &user)) {
        Log()->info("User \"{}\" not found", userName);
        return false;
    }

    if(user.name.empty()) {
        Log()->info("Anonymous user authenticated");
        return true;
    }

    if(user.playPasswordSalt.empty() || user.playPasswordHash.empty()) {
        Log()->error("User \"{}\" has empty salt or hash", userName);
        return false;
    }

    if(!CheckHash(user.playPasswordHashType, pass, user.playPasswordSalt, user.playPasswordHash)) {
        Log()->error("Password hash check failed for user \"{}\"", userName);
        return false;
    }

    Log()->debug("User \"{}\" authenticated", userName);

    return true;
}

bool Server::authorize(
    const UserName& userName,
    RestreamServerLib::Action action,
    const std::string& path,
    bool record)
{
    using namespace RestreamServerLib;

    Log()->trace(
        ">> Server.authorize. user: {}, path: {}, check: {}",
        userName, path, static_cast<int>(action));

    const SourceId sourceId = extractSourceId(path);
    if(sourceId.empty()) {
        Log()->error("Source Id is empty");
        return false;
    }

    const bool allowPlay = _p->config->findUserSource(userName, sourceId);
    const bool allowRecord = _p->config->findDeviceSource(userName, sourceId);
    if(allowPlay && allowRecord) {
        Log()->error("User and Device have the same name: {}", userName);
        return false;
    } else if(!allowPlay && !allowRecord) {
        Log()->error("Unknown restream source \"{}\"", sourceId);
        return false;
    }

    bool authorize = false;
    switch(action) {
    case Action::ACCESS:
    case Action::CONSTRUCT:
        authorize = (!record == allowPlay) || (record == allowRecord);
        break;
    }

    if(authorize) {
        Log()->debug(
            "SourceId \"{}\" is AUTHORIZED for user \"{}\" for \"{}\"",
            sourceId, userName, static_cast<int>(action));
    } else {
        Log()->error(
            "SourceId \"{}\" is NOT authorized for user \"{}\" for \"{}\"",
            sourceId, userName, static_cast<int>(action));
    }

    return authorize;
}

SourceId Server::extractSourceId(const std::string& path) const
{
    SourceId sourceId;

    gchar** tokens = g_strsplit(path.data(), "/", 3);
    for(unsigned i = 0; i < 2 && tokens[i] != NULL; ++i) {
        switch(i) {
            case 0:
                break;
            case 1:
                sourceId = tokens[1];
                break;
        }
    }
    g_strfreev(tokens);

    return sourceId;
}

void Server::firstPlayerConnected(const UserName& userName, const std::string& path)
{
    Log()->trace(
        ">> Server.firstPlayerConnected. path: {}",
        path);

    const SourceId sourceId =  extractSourceId(path);

    ::Server::Config::PlaySource playSource;
    if(!_p->config->findUserSource(userName, sourceId, &playSource)) {
        Log()->critical(
            "fail find PlaySource for {}",
            path);
        return;
    }

    auto pathIt = _p->pathsInfo.find(path);
    if(pathIt == _p->pathsInfo.end()) {
        assert(sourceId == playSource.sourceId);
        _p->pathsInfo.emplace(path,
            PathInfo {
                .deviceId = playSource.deviceId,
                .sourceId = playSource.sourceId,
                .hasPlayers = true,
                .hasRecorder = false,
            });
    } else
        pathIt->second.hasPlayers = true;

    if(_p->firstReaderConnectedCallback)
        _p->firstReaderConnectedCallback(playSource.deviceId, playSource.sourceId);
}

void Server::lastPlayerDisconnected(const std::string& path)
{
    Log()->trace(
        ">> Server.lastPlayerDisconnected. path: {}",
        path);

    auto pathIt = _p->pathsInfo.find(path);
    assert(pathIt != _p->pathsInfo.end());
    if(pathIt == _p->pathsInfo.end())
        return;

    assert(pathIt->second.sourceId == extractSourceId(path));

    assert(pathIt->second.hasPlayers);
    pathIt->second.hasPlayers = false;

    if(_p->lastReaderDisconnectedCallback)
        _p->lastReaderDisconnectedCallback(
            pathIt->second.deviceId,
            pathIt->second.sourceId);

    if(!pathIt->second.hasPlayers && !pathIt->second.hasRecorder)
        _p->pathsInfo.erase(pathIt);
}

void Server::recorderConnected(const UserName& userName, const std::string& path)
{
    Log()->trace(
        ">> Server.recorderConnected. path: {}",
        path);

    SourceId sourceId =  extractSourceId(path);

    ::Server::Config::Source source;
    if(!_p->config->findDeviceSource(userName, sourceId, &source)) {
        Log()->critical(
            "fail find Source for {}",
            path);
        return;
    }

    auto pathIt = _p->pathsInfo.find(path);
    if(pathIt == _p->pathsInfo.end()) {
        _p->pathsInfo.emplace(path,
            PathInfo {
                .deviceId = userName,
                .sourceId = source.id,
                .hasPlayers = false,
                .hasRecorder = true,
            });
    } else {
        assert(!pathIt->second.hasRecorder);
        pathIt->second.hasRecorder = true;
    }
}

void Server::recorderDisconnected(const std::string& path)
{
    Log()->trace(
        ">> Server.recorderDisconnected. path: {}",
        path);

    auto pathIt = _p->pathsInfo.find(path);
    assert(pathIt != _p->pathsInfo.end());
    if(pathIt == _p->pathsInfo.end())
        return;

    assert(pathIt->second.sourceId == extractSourceId(path));

    assert(pathIt->second.hasRecorder);
    pathIt->second.hasRecorder = false;

    if(!pathIt->second.hasPlayers && !pathIt->second.hasRecorder)
        _p->pathsInfo.erase(pathIt);
}

}
