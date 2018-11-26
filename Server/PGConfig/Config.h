#pragma once

#include <memory>

#include "../Config/Config.h"


namespace Server
{

namespace PGConfig
{

using ::Server::Config::Server;
using ::Server::Config::Source;
using ::Server::Config::PlaySource;

class Config : public ::Server::Config::Config
{
public:
    Config();
    ~Config();

    std::unique_ptr<const ::Server::Config::Config> clone() const override;


    const ::Server::Config::Server* serverConfig() const override;


    std::string certificate() const override;


    bool authenticate(X509*, UserName*) const override;
    bool authenticate(GTlsCertificate*, UserName*) const override;


    bool findDevice(const DeviceId&, ::Server::Config::Device* out = nullptr) const override;

    bool findDeviceSource(const DeviceId&, const SourceId&, ::Server::Config::Source* = nullptr) const override;
    void enumDeviceSources(const DeviceId&, const std::function<bool(const Source&)>&) const override;

    bool findUser(const UserName&, ::Server::Config::User* out = nullptr) const override;

    bool findUserSource(const UserName&, const SourceId&, PlaySource* out = nullptr) const override;

private:
    struct Private;
    std::unique_ptr<Private> _p;
};

}

}
