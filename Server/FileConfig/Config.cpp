#include "Config.h"

#include <openssl/pem.h>
#include <libconfig.h>

#undef CHAR_WIDTH
#include <spdlog/fmt/fmt.h>

#include <CxxPtr/GlibPtr.h>

#include "../Config/Log.h"


namespace Server
{

namespace FileConfig
{

Source* Device::addSource(const SourceId& sourceId)
{
    return &sources.emplace(sourceId, Source{.id = sourceId}).first->second;
}

const Source* Device::findSource(const SourceId& sourceId) const
{
    auto it = sources.find(sourceId);
    if(sources.end() != it)
        return &(it->second);

    return nullptr;
}

void Device::enumSources(
    const std::function<bool(const Source&)>& callback) const
{
    for(const auto& pair: sources) {
        if(!callback(pair.second))
            break;
    }
}


const PlaySource* User::addSource(
    DeviceId deviceId,
    SourceId sourceId)
{
    return
        &*playSources.emplace(
            PlaySource{
                .sourceId = sourceId,
                .deviceId = deviceId}).first;
}

const PlaySource* User::findSource(const SourceId& sourceId) const
{
    auto it = playSources.find(PlaySource{.sourceId = sourceId});
    if(playSources.end() != it)
        return &(*it);

    return nullptr;
}


Config::Config()
{
    loadConfig();
    loadCertificates();
}

std::string Config::configDir() const
{
    const gchar* configDir = g_get_user_config_dir();
    if(!configDir) {
        ConfigLog()->critical("Fail get config directory");
        return std::string();
    }

    return configDir;
}

struct LibconfigDestroy
{
    void operator() (config_t* object)
        { config_destroy(object); }
};

typedef
    std::unique_ptr<
        config_t,
        LibconfigDestroy> ConfigDestroy;

void Config::loadDeviceSourceConfig(Device* device, config_setting_t* sourceConfig)
{
    if(!sourceConfig || CONFIG_FALSE == config_setting_is_group(sourceConfig))
        return;

    const char* id;
    const char* uri;
    if(CONFIG_FALSE == config_setting_lookup_string(sourceConfig, "id", &id) ||
        CONFIG_FALSE == config_setting_lookup_string(sourceConfig, "uri", &uri))
    {
        return;
    }

    Source* source = device->addSource(id);
    source->uri = uri;
}

void Config::loadDeviceConfig(config_setting_t* deviceConfig)
{
    if(!deviceConfig || CONFIG_FALSE == config_setting_is_group(deviceConfig))
        return;

    const char* id;
    if(CONFIG_FALSE == config_setting_lookup_string(deviceConfig, "id", &id)) {
        ConfigLog()->warn("Missing device Id. Device skipped.");
        return;
    }

    const char* certificate;
    if(CONFIG_FALSE == config_setting_lookup_string(deviceConfig, "certificate", &certificate)) {
        ConfigLog()->warn(
            "Missing device certificate. Device \"{}\" skipped.",
            id);
        return;
    }

    Device* device = addDevice(id);
    if(!device)
        return;

    device->certificate = certificate;

    config_setting_t* sourcesConfig =
        config_setting_lookup(deviceConfig, "sources");
    if(sourcesConfig && CONFIG_TRUE == config_setting_is_list(sourcesConfig)) {
        const int sourcesCount = config_setting_length(sourcesConfig);
        for(int sourceIdx = 0; sourceIdx < sourcesCount; ++sourceIdx) {
            config_setting_t* sourceConfig =
                config_setting_get_elem(sourcesConfig, sourceIdx);
            loadDeviceSourceConfig(device, sourceConfig);
        }
    }
}

void Config::loadUserSourceConfig(User* user, config_setting_t* sourceConfig)
{
    if(!sourceConfig || CONFIG_FALSE == config_setting_is_group(sourceConfig))
        return;

    const char* device;
    if(CONFIG_FALSE == config_setting_lookup_string(sourceConfig, "device", &device)) {
        ConfigLog()->warn(
            "Missing device Id. User \"{}\" source skipped.",
            user->name
            );
        return;
    }

    const char* source;
    if(CONFIG_FALSE == config_setting_lookup_string(sourceConfig, "source", &source)) {
        ConfigLog()->warn(
            "Missing source Id. User \"{}\" source skipped.",
            user->name
            );
        return;
    }

    user->addSource(device, source);
}

void Config::loadUserConfig(config_setting_t* userConfig)
{
    if(!userConfig || CONFIG_FALSE == config_setting_is_group(userConfig))
        return;

    const char* login;
    if(CONFIG_FALSE == config_setting_lookup_string(userConfig, "login", &login)) {
        ConfigLog()->warn("Missing login. User skipped.");
        return;
    }

    User* user = addUser(login);
    if(!user)
        return;

    config_setting_t* sourcesConfig =
        config_setting_lookup(userConfig, "sources");
    if(sourcesConfig && CONFIG_TRUE == config_setting_is_list(sourcesConfig)) {
        const int sourcesCount = config_setting_length(sourcesConfig);
        for(int sourceIdx = 0; sourceIdx < sourcesCount; ++sourceIdx) {
            config_setting_t* sourceConfig =
                config_setting_get_elem(sourcesConfig, sourceIdx);
            loadUserSourceConfig(user, sourceConfig);
        }
    }
}

void Config::loadConfig()
{
    const std::string configDir = this->configDir();
    if(configDir.empty())
        return;

    config_t config;
    config_init(&config);
    ConfigDestroy ConfigDestroy(&config);

    const std::string configFile =
        fmt::format("{}/{}", configDir, "ipcambox.config").c_str();

    if(!config_read_file(&config, configFile.c_str())) {
        ConfigLog()->critical("Fail load config {}", configFile);
        return;
    }

    _serverConfig.controlServerPort = DEFAULT_CONTROL_SERVER_PORT;
    _serverConfig.staticServerPort = DEFAULT_STATIC_SERVER_PORT;
    _serverConfig.restreamServerPort = DEFAULT_RESTREAM_SERVER_PORT;

    config_setting_t* serverConfig = config_lookup(&config, "server");
    if(serverConfig && CONFIG_TRUE == config_setting_is_group(serverConfig)) {
        const char* serverHost = nullptr;
        if(CONFIG_TRUE == config_setting_lookup_string(serverConfig, "host", &serverHost)) {
            _serverConfig.serverHost = serverHost;
        }
        const char* certificatePath = nullptr;
        if(CONFIG_TRUE == config_setting_lookup_string(serverConfig, "certificate", &certificatePath)) {
            _certificatePath = certificatePath;
        }
        const char* privateKeyPath = nullptr;
        if(CONFIG_TRUE == config_setting_lookup_string(serverConfig, "key", &privateKeyPath)) {
            _privateKeyPath = privateKeyPath;
        }
    }

    if(_serverConfig.serverHost.empty()) {
        ConfigLog()->critical("Missing host name");
        return;
    }
    if(_certificatePath.empty()) {
        ConfigLog()->critical("Missing certificate path");
        return;
    }
    if(_privateKeyPath.empty()) {
        ConfigLog()->critical("Missing private key path");
        return;
    }

    config_setting_t* devicesConfig = config_lookup(&config, "devices");
    if(devicesConfig && CONFIG_TRUE == config_setting_is_list(devicesConfig)) {
        const int deviceCount = config_setting_length(devicesConfig);
        for(int deviceIdx = 0; deviceIdx < deviceCount; ++deviceIdx) {
            config_setting_t* deviceConfig =
                config_setting_get_elem(devicesConfig, deviceIdx);
            loadDeviceConfig(deviceConfig);
        }
    }

    config_setting_t* usersConfig = config_lookup(&config, "users");
    if(usersConfig && CONFIG_TRUE == config_setting_is_list(usersConfig)) {
        const int usersCount = config_setting_length(usersConfig);
        for(int userIdx = 0; userIdx < usersCount; ++userIdx) {
            config_setting_t* userConfig =
                config_setting_get_elem(usersConfig, userIdx);
            loadUserConfig(userConfig);
        }
    }
}

static std::string FullPath(const std::string& configDir, const std::string& path)
{
    std::string fullPath;
    if(!g_path_is_absolute(path.c_str())) {
        gchar* tmpPath =
            g_build_filename(configDir.c_str(), path.c_str(), NULL);
        fullPath = tmpPath;
        g_free(tmpPath);
    } else
        fullPath = path;

    return fullPath;
}

std::string Config::certificate() const
{
    const std::string configDir = this->configDir();
    if(configDir.empty() || _certificatePath.empty())
        return _certificate;

    const std::string certificatePath =
        FullPath(configDir, _certificatePath);
    const std::string privateKeyPath =
        FullPath(configDir, _privateKeyPath);

    gchar* certificate;
    gchar* privateKey;
    if(g_file_get_contents(certificatePath.c_str(), &certificate, nullptr, nullptr)) {
        if(g_file_get_contents(privateKeyPath.c_str(), &privateKey, nullptr, nullptr)) {
            _certificate = certificate;
            _certificate += "\n";
            _certificate += privateKey;

            g_free(privateKey);
        }
        g_free(certificate);
    }

    return _certificate;
}

void Config::loadCertificates()
{
    _allowedClients.reset(X509_STORE_new());
    X509_STORE* allowedClients = _allowedClients.get();
    if(!allowedClients) {
        ConfigLog()->error("X509_STORE_new failed");
        return;
    }

    enumDevices(
        [this] (const ::Server::Config::Device& device) {
            if(device.certificate.empty()) {
                ConfigLog()->warn("Empty device certificate");
                return;
            }

            BIOPtr deviceCertBioPtr(BIO_new(BIO_s_mem()));
            BIO* deviceCertBio = deviceCertBioPtr.get();
            if(!deviceCertBio) {
                ConfigLog()->error("BIO_new failed");
                return;
            }

            if(BIO_write(
                deviceCertBio,
                device.certificate.data(),
                device.certificate.size()) <= 0)
            {
                ConfigLog()->error("BIO_write failed");
                return;
            }

            X509Ptr deviceCertPtr(PEM_read_bio_X509(deviceCertBio, NULL, NULL, NULL));
            X509* deviceCert = deviceCertPtr.get();
            if(!deviceCert) {
                ConfigLog()->error("Failed parse device box certificate");
                return;
            }

            if(!X509_STORE_add_cert(_allowedClients.get(), deviceCert)) {
                ConfigLog()->error("X509_STORE_add_cert failed");
            }
        }
    );
}

bool Config::authenticate(X509* cert, std::string* id) const
{
    X509_STORE* allowedClients = _allowedClients.get();
    if(!allowedClients)
        return false;

    X509_STORE_CTXPtr ctxPtr(X509_STORE_CTX_new());
    X509_STORE_CTX* ctx = ctxPtr.get();
    if(!ctx) {
        ConfigLog()->error("X509_STORE_CTX_new failed");
        return false;
    }

    if(!X509_STORE_CTX_init(ctx, allowedClients, cert, NULL))
        return false;

    if(!X509_verify_cert(ctx)) {
        ConfigLog()->error("Client certificate is NOT allowed");
        return false;
    }

    X509_NAME* subject = X509_get_subject_name(cert);
    if(!subject) {
        ConfigLog()->error("X509_get_subject_name failed");
        return false;
    }

    const int nameIndex = X509_NAME_get_index_by_NID(subject, NID_commonName, -1);
    if(nameIndex < 0) {
        ConfigLog()->error("X509_NAME_get_entry failed");
        return false;
    }

    X509_NAME_ENTRY* nameEntry = X509_NAME_get_entry(subject, nameIndex);
    if(!nameEntry) {
        ConfigLog()->error("X509_NAME_get_entry failed");
        return false;
    }

    ASN1_STRING* asn1Name = X509_NAME_ENTRY_get_data(nameEntry);
    if(!asn1Name) {
        ConfigLog()->error("X509_NAME_ENTRY_get_data failed");
        return false;
    }

    const unsigned char* commonName = ASN1_STRING_data(asn1Name);
    if(!commonName) {
        ConfigLog()->error("ASN1_STRING_get0_data failed");
        return false;
    }

    ConfigLog()->info("Client certificate is allowed. Subject: {}", commonName);

    if(id)
        *id = reinterpret_cast<const std::string::value_type*>(commonName);

    return true;
}

bool Config::authenticate(GTlsCertificate* cert, UserName* name) const
{
    BIOPtr certBioPtr(BIO_new(BIO_s_mem()));
    BIO* certBio = certBioPtr.get();
    if(!certBio) {
        ConfigLog()->error("BIO_new failed");
        return false;
    }

    gchar* pemCertificate;
    g_object_get(cert, "certificate-pem", &pemCertificate, NULL);
    if(!pemCertificate) {
        ConfigLog()->error("certificate-pem access failed");
        return false;
    }

    GCharPtr pemCertificatePtr(pemCertificate);

    if(BIO_puts(certBio, pemCertificate) <= 0) {
        ConfigLog()->error("BIO_write failed");
        return false;
    }

    X509Ptr x509CertPtr(PEM_read_bio_X509(certBio, NULL, NULL, NULL));
    X509* x509Cert = x509CertPtr.get();
    if(!x509Cert) {
        ConfigLog()->error("Failed parse client certificate");
        return false;
    }

    return authenticate(x509Cert, name);
}

std::unique_ptr<const ::Server::Config::Config> Config::clone() const
{
    return std::make_unique<Config>();
}

const Server* Config::serverConfig() const
{
    return &_serverConfig;
}

Device* Config::addDevice(const DeviceId& deviceId)
{
    return &_devices.emplace(deviceId, deviceId).first->second;
}

const Device* Config::findDevice(const DeviceId& deviceId) const
{
    auto it = _devices.find(deviceId);
    if(_devices.end() != it)
        return &it->second;

    return nullptr;
}

bool Config::findDevice(const DeviceId& deviceId, ::Server::Config::Device* out) const
{
    const Device* device = findDevice(deviceId);
    if(!device)
        return false;

    if(out)
        *out = *device;

    return true;
}

void Config::enumDevices(const std::function<void(const ::Server::Config::Device&)>& callback) const
{
    for(const auto& pair : _devices)
        callback(pair.second);
}

bool Config::findDeviceSource(
    const DeviceId& deviceId,
    const SourceId& sourceId,
    ::Server::Config::Source* out) const
{
    const Device* device = findDevice(deviceId);
    if(!device)
        return false;

    const Source* source = device->findSource(sourceId);
    if(!source)
        return false;

    if(out)
        *out = *source;

    return true;
}

void Config::enumDeviceSources(
    const DeviceId& deviceId,
    const std::function<bool(const Source&)>& callback) const
{
    const Device* device = findDevice(deviceId);
    if(!device)
        return;

    device->enumSources(callback);
}

User* Config::addUser(const UserName& name)
{
    return &_users.emplace(name, name).first->second;
}

const User* Config::findUser(const UserName& name) const
{
    auto it = _users.find(name);
    if(_users.end() != it)
        return &it->second;

    return nullptr;
}

bool Config::findUser(const UserName& name, ::Server::Config::User* out) const
{
    const User* user = findUser(name);
    if(!user)
        return false;

    if(out)
        *out = *user;

    return true;
}

bool Config::findUserSource(
    const UserName& name,
    const SourceId& sourceId,
    PlaySource* out) const
{
    const User* user = findUser(name);
    if(!user)
        return false;

    const PlaySource* source = user->findSource(sourceId);
    if(!source)
        return false;

    if(out)
        *out = *source;

    return true;
}

}

}
