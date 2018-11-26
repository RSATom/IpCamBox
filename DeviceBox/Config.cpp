#include "Config.h"

#include <glib.h>


namespace DeviceBox
{

bool Config::empty() const
{
    return _sources.empty() && _dropboxToken.empty();
}

void Config::clear()
{
    _sources.clear();
    _dropboxToken.clear();
}

bool Config::loadSourceConfig(const Protocol::VideoSource& config, SourceConfig* outConfig)
{
    // FIXME! use better place for temp files
    GError* error = nullptr;
    gchar* tmp = g_dir_make_tmp(NULL, &error);
    if(error) {
        g_clear_error(&error);
        return false;
    }

    outConfig->id = config.id();
    outConfig->uri = config.uri();
    outConfig->user = config.user();
    outConfig->password = config.password();
    outConfig->archivePath = tmp;
    outConfig->desiredFileSize = 1 * 1024 * 1024;

    outConfig->dropboxArchivePath = "/" + config.id() + "/"; // FIXME! в целях безопасности возможно не стоит использовать id в путях
    outConfig->dropboxMaxStorage = config.dropboxmaxstorage() * 1024 * 1024;

    g_free(tmp);

    return true;
}

void Config::loadDropboxConfig(const Protocol::DropboxConfig& config)
{
    _dropboxToken = config.token();
}

void Config::loadConfig(const Protocol::ClientConfig& config)
{
    for(const Protocol::VideoSource& source: config.sources()) {
        SourceConfig sourceConfig;
        if(_sources.find(source.id()) == _sources.end() &&
           loadSourceConfig(source, &sourceConfig))
        {
            _sources.emplace(source.id(), sourceConfig);
        }
    }

    if(config.has_dropbox()) {
        loadDropboxConfig(config.dropbox());
    }
}

void Config::updateConfig(const Protocol::ClientConfig& config)
{
    assert(false);
}

void Config::enumSources(const std::function<bool (const SourceConfig&)>& cb)
{
    for(auto& pair: _sources) {
        const SourceConfig& source = pair.second;
        if(!cb(source))
            break;
    }
}

bool Config::findSource(const std::string& id, const std::function<void (const SourceConfig&)>& cb)
{
    auto it = _sources.find(id);
    if( it != _sources.end()) {
        cb(it->second);
        return true;
    }

    return false;
}

std::string Config::dropboxToken() const
{
    return _dropboxToken;
}

}
