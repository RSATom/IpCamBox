#pragma once

#include <memory>
#include <unordered_map>
#include <set>
#include <functional>

#include <openssl/x509.h>
#include <gio/gio.h>

#include <Common/Config.h>
#include <Common/CommonTypes.h>
#include <Common/Hash.h>


namespace Server
{

namespace Config
{

struct Source
{
    SourceId id;

    std::string uri;

    unsigned dropboxMaxStorage; // in megabytes
};

struct Device
{
    DeviceId id;

    std::string certificate;

    std::string dropboxToken;
};


struct PlaySource
{
    SourceId sourceId;
    DeviceId deviceId;
};

struct User
{
    UserName name;

    std::string playPasswordSalt;
    HashType playPasswordHashType;
    std::string playPasswordHash;
};


struct Server
{
    Host serverHost;

    Port controlServerPort;
    Port staticServerPort;
    Port restreamServerPort;

    URL noSignalSplashSource;
};

struct Config
{
    virtual ~Config() {}

    // to use in some new thread
    virtual std::unique_ptr<const Config> clone() const = 0;

    virtual const Server* serverConfig() const = 0;

    // should include private key and intermediate certs
    virtual std::string certificate() const = 0;

    virtual bool authenticate(X509*, UserName*) const = 0;
    virtual bool authenticate(GTlsCertificate*, UserName*) const = 0;


    virtual bool findDevice(const DeviceId&, Device* out = nullptr) const = 0;

    virtual bool findDeviceSource(const DeviceId&, const SourceId&, Source* out = nullptr) const = 0;
    virtual void enumDeviceSources(const DeviceId&, const std::function<bool(const Source&)>&) const = 0;


    virtual bool findUser(const UserName&, User* out = nullptr) const = 0;

    virtual bool findUserSource(const UserName&, const SourceId&, PlaySource* out = nullptr) const = 0;
};

}

}
