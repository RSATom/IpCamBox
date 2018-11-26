#pragma once

#include <CxxPtr/OpenSSLPtr.h>

#include "Config.h"


namespace Server
{

namespace MemoryConfig
{

using ::Server::Config::Server;
using ::Server::Config::Source;
using ::Server::Config::PlaySource;

class Device: public ::Server::Config::Device
{
public:
    Device(const DeviceId& deviceId) : ::Server::Config::Device{deviceId} {}

    Source* addSource(const SourceId&);

    const Source* findSource(const SourceId&) const;
    void enumSources(const std::function<bool(const Source&)>&) const;

private:
    std::unordered_map<SourceId, Source> sources;
};


struct LessBySourceId
{
    bool operator () (
        const ::Server::Config::PlaySource& x,
        const ::Server::Config::PlaySource& y) const
            { return x.sourceId < y.sourceId; }
};

class User : public ::Server::Config::User
{
public:
    User(const UserName& name) : ::Server::Config::User{name} {}

    const ::Server::Config::PlaySource* addSource(
        DeviceId deviceId,
        SourceId sourceId);

    const ::Server::Config::PlaySource* findSource(const SourceId&) const;

private:
    std::set<::Server::Config::PlaySource, LessBySourceId> playSources;
};


class Config : public ::Server::Config::Config
{
public:
    Config();

    const ::Server::Config::Server* serverConfig() const override;


    std::unique_ptr<const ::Server::Config::Config> clone() const override;


    std::string certificate() const override;


    bool authenticate(X509*, UserName*) const override;
    bool authenticate(GTlsCertificate*, UserName*) const override;


    const Device* findDevice(const DeviceId&) const;
    bool findDevice(const DeviceId&, ::Server::Config::Device* out) const override;
    void enumDevices(const std::function<void(const ::Server::Config::Device&)>&) const;

    bool findDeviceSource(const DeviceId&, const SourceId&, ::Server::Config::Source*) const override;
    void enumDeviceSources(const DeviceId&, const std::function<bool(const Source&)>&) const override;

    const User* findUser(const UserName&) const;
    bool findUser(const UserName&, ::Server::Config::User* out) const override;

    bool findUserSource(const UserName&, const SourceId&, PlaySource* out) const override;

private:
    Device* addDevice(const DeviceId&);
    User* addUser(const UserName&);

    void loadCertificates();

private:
    Server _serverConfig;

    std::unordered_map<DeviceId, Device> _devices;
    std::unordered_map<UserName, User> _users;

    X509_STOREPtr _allowedClients;
};

}

}
