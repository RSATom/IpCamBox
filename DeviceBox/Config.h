#pragma once

#include <map>
#include <functional>

#include "Protocol/protocol.h"

#include "SourceConfig.h"


namespace DeviceBox
{

class Config
{
public:
    bool empty() const;
    void clear();

    void loadConfig(const Protocol::ClientConfig&);
    void updateConfig(const Protocol::ClientConfig&);

    void enumSources(const std::function<bool (const SourceConfig&)>&);
    bool findSource(const std::string& id, const std::function<void (const SourceConfig&)>&);

    std::string dropboxToken() const;

private:
    bool loadSourceConfig(const Protocol::VideoSource& config, SourceConfig* outConfig);
    void loadDropboxConfig(const Protocol::DropboxConfig& config);

private:
    std::map<std::string, SourceConfig> _sources;
    std::string _dropboxToken;
};

}
