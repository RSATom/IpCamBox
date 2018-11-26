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
            std::placeholders::_2);
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
        std::bind(&Server::firstPlayerConnected, this, std::placeholders::_1);
    callbacks.lastPlayerDisconnected =
        std::bind(&Server::lastPlayerDisconnected, this, std::placeholders::_1);
    callbacks.recorderConnected =
        std::bind(&Server::recorderConnected, this, std::placeholders::_1);
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

bool Server::authenticationRequired(GstRTSPMethod method, const std::string& path)
{
    Log()->trace(">> Server.authenticationRequired. url: {}", path);

    const std::string sourceId = extractSourceId(path);
    if(sourceId.empty())
        return true;

    if(_p->config->findUserSource(UserName(), sourceId)) {
        Log()->trace("SourceId \"{}\" DOES NOT require authentication as anonymous", sourceId);
        return false;
    }

    Log()->trace("SourceId \"{}\" REQUIRE authentication", sourceId);

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

    const std::string sourceId = extractSourceId(path);
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

std::string Server::extractSourceId(const std::string& path) const
{
    std::string sourceName;

    gchar** tokens = g_strsplit(path.data(), "/", 3);
    for(unsigned i = 0; i < 2 && tokens[i] != NULL; ++i) {
        switch(i) {
            case 0:
                break;
            case 1:
                sourceName = tokens[1];
                break;
        }
    }
    g_strfreev(tokens);

    return sourceName;
}

void Server::firstPlayerConnected(const std::string& path)
{
    Log()->trace(
        ">> Server.firstPlayerConnected. path: {}",
        path);
}

void Server::lastPlayerDisconnected(const std::string& path)
{
    Log()->trace(
        ">> Server.lastPlayerDisconnected. path: {}",
        path);
}

void Server::recorderConnected(const std::string& path)
{
    Log()->trace(
        ">> Server.recorderConnected. path: {}",
        path);
}

void Server::recorderDisconnected(const std::string& path)
{
    Log()->trace(
        ">> Server.recorderDisconnected. path: {}",
        path);
}

}
