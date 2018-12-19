#include "MemoryConfig.h"

#include <openssl/pem.h>

#undef CHAR_WIDTH
#include <spdlog/fmt/fmt.h>

#include <CxxPtr/GlibPtr.h>

#include "Log.h"
#include "Common/Keys.h"


namespace Server
{

namespace MemoryConfig
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
    _serverConfig.serverHost = DEFAULT_SERVER_HOST;
    _serverConfig.controlServerPort = DEFAULT_CONTROL_SERVER_PORT;
    _serverConfig.staticServerPort = DEFAULT_STATIC_SERVER_PORT;
    _serverConfig.restreamServerPort = DEFAULT_RESTREAM_SERVER_PORT;


    Device* deviceConfig = addDevice("device1");
    deviceConfig->certificate = TestClientCertificate;


    Source* bars = deviceConfig->addSource("bars");
    bars->uri =
        fmt::format(
            "rtsp://{}:{}/bars",
            _serverConfig.serverHost,
            _serverConfig.staticServerPort);
    bars->dropboxMaxStorage = 0;

    Source* dlink = deviceConfig->addSource("dlink931");
    dlink->uri = "http://172.27.39.11/h264.flv";
    dlink->dropboxMaxStorage = 0;

    User* anonymous = addUser(UserName());
    anonymous->addSource(deviceConfig->id, bars->id);
    anonymous->addSource(deviceConfig->id, dlink->id);

    loadCertificates();
}

std::string Config::certificate() const
{
    static const std::string certificate =
        std::string(ServerCertificate) + ServerKey + ServerFullChain;

    return certificate;
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

bool Config::authenticate(X509* cert, std::string* name) const
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

    if(name)
        *name = reinterpret_cast<const std::string::value_type*>(commonName);

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
