#include "Config.h"

#include <arpa/inet.h>

#include <openssl/pem.h>
#include <openssl/x509.h>

#include <CxxPtr/GlibPtr.h>
#include <CxxPtr/OpenSSLPtr.h>

#include "../Config/Log.h"
#include "PGPtr.h"


namespace Server
{

namespace PGConfig
{

struct Config::Private
{
    PGconnPtr _connPtr;

    ::Server::Config::Server _server;

    PGconn* checkConnected();

    bool isDeviceExists(const DeviceId&);
    bool findDevice(const DeviceId&, ::Server::Config::Device* out);

    bool isDeviceSourceExists(const DeviceId&, const SourceId&);
    bool findDeviceSource(const DeviceId&, const SourceId&, ::Server::Config::Source* out);
    void enumDeviceSources(const DeviceId&, const std::function<bool(const Source&)>&);

    bool isUserExists(const UserName&);
    bool findUser(const UserName&, ::Server::Config::User* out);

    bool isUserSourceExists(const UserName&, const SourceId&);
    bool findUserSource(const UserName&, const SourceId&, ::Server::Config::PlaySource* out);
};

PGconn* Config::Private::checkConnected()
{
    if(!_connPtr)
        _connPtr.reset(PQconnectdb("dbname=restreamer"));

    PGconn* conn = _connPtr.get();
    if(PQstatus(conn) != CONNECTION_OK) {
        PQreset(conn); // try restore connection. FIXME! maybe should use some timeout
    }

    if(PQstatus(conn) == CONNECTION_OK) {
        return conn;
    } else {
        ConfigLog()->critical(
            "Failed to connect to config db: {}",
            PQerrorMessage(conn));
        return nullptr;
    }
}

bool Config::Private::isDeviceExists(const DeviceId& deviceId)
{
    if(deviceId.empty())
        return false;

    PGconn* conn = checkConnected();
    if(!conn)
        return false;

    const char* paramValues[] =
    {
        deviceId.data()
    };
    const int paramLengths[] =
    {
        static_cast<int>(deviceId.size())
    };

    PGresultPtr resultPtr(
        PQexecParams(conn,
            "select true "
            "from DEVICES "
            "where ID = $1 "
            "limit 1", 1, NULL, paramValues, paramLengths, NULL, 1));
    if(PQresultStatus(resultPtr.get()) != PGRES_TUPLES_OK) {
        ConfigLog()->critical(
            "Failed to retreive device existance: {}",
            PQresultErrorMessage(resultPtr.get()));
        return false;
    }

    PGresult* result = resultPtr.get();
    const int rowCount = PQntuples(result);

    return rowCount > 0;
}

bool Config::Private::findDevice(
    const DeviceId& deviceId,
    ::Server::Config::Device* out)
{
    if(deviceId.empty())
        return false;

    if(!out)
        return isDeviceExists(deviceId);

    PGconn* conn = checkConnected();
    if(!conn)
        return false;

    const char* paramValues[] =
    {
        deviceId.data()
    };
    const int paramLengths[] =
    {
        static_cast<int>(deviceId.size())
    };

    PGresultPtr resultPtr(
        PQexecParams(conn,
            "select ID::text, CERTIFICATE, DROPBOX_TOKEN "
            "from DEVICES "
            "where ID = $1 "
            "limit 1", 1, NULL, paramValues, paramLengths, NULL, 1));
    if(PQresultStatus(resultPtr.get()) != PGRES_TUPLES_OK) {
        ConfigLog()->critical(
            "Failed to retreive device info: {}",
            PQresultErrorMessage(resultPtr.get()));
        return false;
    }

    PGresult* result = resultPtr.get();
    const int rowCount = PQntuples(result);

    if(rowCount == 0)
        return false;

    const char* ID = PQgetvalue(result, 0, 0);
    const char* CERTIFICATE = PQgetvalue(result, 0, 1);
    const char* DROPBOX_TOKEN = PQgetvalue(result, 0, 2);

    out->id.assign(ID);
    out->certificate.assign(CERTIFICATE);
    out->dropboxToken.assign(DROPBOX_TOKEN);

    return true;
}

bool Config::Private::isDeviceSourceExists(
    const DeviceId& deviceId, const SourceId& sourceId)
{
    if(deviceId.empty() || sourceId.empty())
        return false;

    PGconn* conn = checkConnected();
    if(!conn)
        return false;

    const char* paramValues[] =
    {
        sourceId.data(),
        deviceId.data(),
    };
    const int paramLengths[] =
    {
        static_cast<int>(sourceId.size()),
        static_cast<int>(deviceId.size()),
    };

    PGresultPtr resultPtr(
        PQexecParams(conn,
            "select true "
            "from SOURCES "
            "where ID = $1 and DEVICE_ID = $2 "
            "limit 1", 2, NULL, paramValues, paramLengths, NULL, 1));
    if(PQresultStatus(resultPtr.get()) != PGRES_TUPLES_OK) {
        ConfigLog()->critical(
            "Failed to retreive device source existance: {}",
            PQresultErrorMessage(resultPtr.get()));
        return false;
    }

    PGresult* result = resultPtr.get();
    const int rowCount = PQntuples(result);

    return rowCount > 0;
}

bool Config::Private::findDeviceSource(
    const DeviceId& deviceId,
    const SourceId& sourceId,
    ::Server::Config::Source* out)
{
    if(deviceId.empty() || sourceId.empty())
        return false;

    if(!out)
        return isDeviceExists(deviceId);

    PGconn* conn = checkConnected();
    if(!conn)
        return false;

    const char* paramValues[] =
    {
        sourceId.data(),
        deviceId.data(),
    };
    const int paramLengths[] =
    {
        static_cast<int>(sourceId.size()),
        static_cast<int>(deviceId.size()),
    };

    PGresultPtr resultPtr(
        PQexecParams(conn,
            "select ID::text, URI, DROPBOX_STORAGE "
            "from SOURCES "
            "where ID = $1 and DEVICE_ID = $2 "
            "limit 1", 2, NULL, paramValues, paramLengths, NULL, 1));
    if(PQresultStatus(resultPtr.get()) != PGRES_TUPLES_OK) {
        ConfigLog()->critical(
            "Failed to retreive device source info: {}",
            PQresultErrorMessage(resultPtr.get()));
        return false;
    }

    PGresult* result = resultPtr.get();
    const int rowCount = PQntuples(result);

    if(rowCount == 0)
        return false;

    const char* ID = PQgetvalue(result, 0, 0);
    const char* URI = PQgetvalue(result, 0, 1);
    const void* DROPBOX_STORAGE =
        PQgetisnull(result, 0, 2) ?
            nullptr :
            PQgetvalue(result, 0, 2);

    out->id.assign(ID);
    out->uri.assign(URI);
    out->dropboxMaxStorage =
        DROPBOX_STORAGE ?
        ntohl(*static_cast<const uint32_t*>(DROPBOX_STORAGE)) :
        0;

    return true;
}

void Config::Private::enumDeviceSources(
    const DeviceId& deviceId, const std::function<bool(const Source&)>& callback)
{
    PGconn* conn = checkConnected();
    if(!conn)
        return;

    const char* paramValues[] =
    {
        deviceId.data(),
    };
    const int paramLengths[] =
    {
        static_cast<int>(deviceId.size()),
    };

    PGresultPtr resultPtr(
        PQexecParams(conn,
            "select ID::text, URI, DROPBOX_STORAGE "
            "from SOURCES "
            "where DEVICE_ID = $1 "
            "limit 1", 1, NULL, paramValues, paramLengths, NULL, 1));
    if(PQresultStatus(resultPtr.get()) != PGRES_TUPLES_OK) {
        ConfigLog()->critical(
            "Failed to retreive device sources info: {}",
            PQresultErrorMessage(resultPtr.get()));
        return;
    }

    PGresult* result = resultPtr.get();
    const int rowCount = PQntuples(result);

    if(rowCount == 0)
        return;

    for(int i = 0; i < rowCount; ++i) {
        const char* ID = PQgetvalue(result, i, 0);
        const char* URI = PQgetvalue(result, i, 1);
        const void* DROPBOX_STORAGE =
            PQgetisnull(result, i, 2) ?
                nullptr :
                PQgetvalue(result, i, 2);

        Source source;
        source.id.assign(ID);
        source.uri.assign(URI);
        source.dropboxMaxStorage =
            DROPBOX_STORAGE ?
                ntohl(*static_cast<const uint32_t*>(DROPBOX_STORAGE)) :
                0;

        if(!callback(source))
            return;
    }
}

bool Config::Private::isUserExists(const UserName& userName)
{
    PGconn* conn = checkConnected();
    if(!conn)
        return false;

    const char* paramValues[] =
    {
        userName.data()
    };
    const int paramLengths[] =
    {
        static_cast<int>(userName.size())
    };

    PGresultPtr resultPtr(
        PQexecParams(conn,
            "select true "
            "from USERS "
            "where LOGIN = $1 "
            "limit 1", 1, NULL, paramValues, paramLengths, NULL, 1));
    if(PQresultStatus(resultPtr.get()) != PGRES_TUPLES_OK) {
        ConfigLog()->critical(
            "Failed to retreive user existance: {}",
            PQresultErrorMessage(resultPtr.get()));
        return false;
    }

    PGresult* result = resultPtr.get();
    const int rowCount = PQntuples(result);

    return rowCount > 0;
}

bool Config::Private::findUser(const UserName& userName, ::Server::Config::User* out)
{
    if(!out)
        return isUserExists(userName);

    PGconn* conn = checkConnected();
    if(!conn)
        return false;

    const char* paramValues[] =
    {
        userName.data()
    };
    const int paramLengths[] =
    {
        static_cast<int>(userName.size())
    };

    PGresultPtr resultPtr(
        PQexecParams(conn,
            "select LOGIN, SALT, HASH_TYPE::smallint, PASSWORD_HASH "
            "from USERS "
            "where LOGIN = $1 "
            "limit 1", 1, NULL, paramValues, paramLengths, NULL, 1));
    if(PQresultStatus(resultPtr.get()) != PGRES_TUPLES_OK) {
        ConfigLog()->critical(
            "Failed to retreive user info: {}",
            PQresultErrorMessage(resultPtr.get()));
        return false;
    }

    PGresult* result = resultPtr.get();
    const int rowCount = PQntuples(result);

    if(rowCount == 0)
        return false;

    const char* LOGIN = PQgetvalue(result, 0, 0);
    const char* SALT = PQgetvalue(result, 0, 1);
    const void* HASH_TYPE = PQgetvalue(result, 0, 2);
    const char* PASSWORD_HASH = PQgetvalue(result, 0, 3);

    const uint16_t hashType=
        ntohs(*static_cast<const uint16_t*>(HASH_TYPE));
    switch(hashType) {
    case 0:
        out->playPasswordHashType = HashType::SHA1;
        break;
    case 1:
        out->playPasswordHashType = HashType::SHA256;
        break;
    default:
        ConfigLog()->error(
            "Unknown password hash type: {}",
            hashType);
        return false;
    }

    out->name.assign(LOGIN);
    out->playPasswordSalt.assign(SALT);
    out->playPasswordHash.assign(PASSWORD_HASH);

    return true;
}

bool Config::Private::isUserSourceExists(
    const UserName& userName,
    const SourceId& sourceId)
{
    if(sourceId.empty())
        return false;

    PGconn* conn = checkConnected();
    if(!conn)
        return false;

    const char* paramValues[] =
    {
        userName.data(),
        sourceId.data(),
    };
    const int paramLengths[] =
    {
        static_cast<int>(userName.size()),
        static_cast<int>(sourceId.size()),
    };

    PGresultPtr resultPtr(
        PQexecParams(conn,
            "select true "
            "from USERS u, RIGHTS r "
            "where u.LOGIN = $1 and u.ID = r.USER_ID and r.SOURCE_ID = $2 "
            "limit 1", 2, NULL, paramValues, paramLengths, NULL, 1));
    if(PQresultStatus(resultPtr.get()) != PGRES_TUPLES_OK) {
        ConfigLog()->critical(
            "Failed to retreive user source existance: {}",
            PQresultErrorMessage(resultPtr.get()));
        return false;
    }

    PGresult* result = resultPtr.get();
    const int rowCount = PQntuples(result);

    return rowCount > 0;
}

bool Config::Private::findUserSource(
    const UserName& userName,
    const SourceId& sourceId,
    ::Server::Config::PlaySource* out)
{
    if(sourceId.empty())
        return false;

    if(!out)
        return isUserSourceExists(userName, sourceId);

    PGconn* conn = checkConnected();
    if(!conn)
        return false;

    const char* paramValues[] =
    {
        userName.data(),
        sourceId.data(),
    };
    const int paramLengths[] =
    {
        static_cast<int>(userName.size()),
        static_cast<int>(sourceId.size()),
    };

    PGresultPtr resultPtr(
        PQexecParams(conn,
            "select r.SOURCE_ID::text, s.DEVICE_ID::text "
            "from USERS u, RIGHTS r, SOURCES s "
            "where u.LOGIN = $1 and u.ID = r.USER_ID and "
            "r.SOURCE_ID = $2 and r.SOURCE_ID = s.ID "
            "limit 1", 2, NULL, paramValues, paramLengths, NULL, 1));
    if(PQresultStatus(resultPtr.get()) != PGRES_TUPLES_OK) {
        ConfigLog()->critical(
            "Failed to retreive user source info: {}",
            PQresultErrorMessage(resultPtr.get()));
        return false;
    }

    PGresult* result = resultPtr.get();
    const int rowCount = PQntuples(result);

    if(rowCount == 0)
        return false;

    const char* SOURCE_ID = PQgetvalue(result, 0, 0);
    const char* DEVICE_ID = PQgetvalue(result, 0, 1);

    out->sourceId.assign(SOURCE_ID);
    out->deviceId.assign(DEVICE_ID);

    return true;
}


Config::Config() :
    _p(new Private)
{
}

Config::~Config()
{
    _p.reset();
}

std::unique_ptr<const ::Server::Config::Config> Config::clone() const
{
    return std::make_unique<Config>();
}

const ::Server::Config::Server* Config::serverConfig() const
{
    if(!_p->_server.serverHost.empty())
        return &_p->_server;

    PGconn* conn = _p->checkConnected();
    if(!conn)
        return nullptr;

    PGresultPtr resultPtr(
        PQexecParams(conn,
            "select HOST, CONTROL_PORT, STATIC_PORT, RESTREAM_PORT "
            "from SERVER "
            "limit 1", 0, NULL, NULL, NULL, NULL, 1));
    if(PQresultStatus(resultPtr.get()) != PGRES_TUPLES_OK) {
        ConfigLog()->critical("Failed to retreive server config");
        return nullptr;
    }

    PGresult* result = resultPtr.get();
    const int rowCount = PQntuples(result);

    if(0 == rowCount) {
        ConfigLog()->critical("Server config is missing");
        return nullptr;
    }

    const char* HOST = PQgetvalue(result, 0, 0);
    const void* CONTROL_PORT = PQgetvalue(result, 0, 1);
    const void* STATIC_PORT = PQgetvalue(result, 0, 2);
    const void* RESTREAM_PORT = PQgetvalue(result, 0, 3);

    _p->_server.serverHost = HOST;
    _p->_server.controlServerPort =
        ntohs(*static_cast<const uint16_t*>(CONTROL_PORT));
    _p->_server.staticServerPort =
        ntohs(*static_cast<const uint16_t*>(STATIC_PORT));
    _p->_server.restreamServerPort=
        ntohs(*static_cast<const uint16_t*>(RESTREAM_PORT));

    return &_p->_server;
}

std::string Config::certificate() const
{
    PGconn* conn = _p->checkConnected();
    if(!conn)
        return nullptr;

    PGresultPtr resultPtr(
        PQexecParams(conn,
            "select CERTIFICATE "
            "from SERVER "
            "limit 1", 0, NULL, NULL, NULL, NULL, 1));
    if(PQresultStatus(resultPtr.get()) != PGRES_TUPLES_OK) {
        ConfigLog()->critical("Failed to retreive certificate");
        return std::string();
    }

    PGresult* result = resultPtr.get();
    const int rowCount = PQntuples(result);

    if(0 == rowCount) {
        ConfigLog()->critical("Server certificate is missing");
        return std::string();
    }

    const char* CERTIFICATE = PQgetvalue(result, 0, 0);

    return std::string(CERTIFICATE);
}

X509Ptr makeCertificate(const char* pemCertificate)
{
    X509Ptr nullPtr;

    BIOPtr certBioPtr(BIO_new(BIO_s_mem()));
    BIO* certBio = certBioPtr.get();
    if(!certBio) {
        ConfigLog()->error("BIO_new failed");
        return std::move(nullPtr);
    }

    if(BIO_puts(certBio, pemCertificate) <= 0) {
        ConfigLog()->error("BIO_write failed");
        return std::move(nullPtr);
    }

    X509Ptr x509CertPtr(PEM_read_bio_X509(certBio, NULL, NULL, NULL));
    X509* x509Cert = x509CertPtr.get();
    if(!x509Cert) {
        ConfigLog()->error("Failed parse client certificate");
        return std::move(nullPtr);
    }

    return std::move(x509CertPtr);
}

bool Config::authenticate(X509* cert, UserName* userName) const
{
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

    ::Server::Config::Device device;
    if(!_p->findDevice(reinterpret_cast<const std::string::value_type*>(commonName), &device)) {
        ConfigLog()->error(
            "Failed find device by certificate. Name \"{}\"",
            commonName);
        return false;
    }

    X509Ptr x509DeviceCertPtr = makeCertificate(device.certificate.data());
    X509* x509DeviceCert = x509DeviceCertPtr.get();
    if(!x509DeviceCert)
        return false;

    if(X509_cmp(cert, x509DeviceCert) != 0) {
        ConfigLog()->info(
            "Received certificate differ from certificate stored on server. Name {}",
            commonName);
        return false;
    }

    ConfigLog()->info("Client certificate is allowed. Subject: {}", commonName);

    if(userName)
        *userName = reinterpret_cast<const std::string::value_type*>(commonName);

    return true;
}

bool Config::authenticate(GTlsCertificate* cert, UserName* userName) const
{
    gchar* pemCertificate;
    g_object_get(cert, "certificate-pem", &pemCertificate, NULL);
    if(!pemCertificate) {
        ConfigLog()->error("certificate-pem access failed");
        return false;
    }

    GCharPtr pemCertificatePtr(pemCertificate);

    X509Ptr x509CertPtr = makeCertificate(pemCertificate);
    X509* x509Cert = x509CertPtr.get();
    if(!x509Cert)
        return false;

    return authenticate(x509Cert, userName);
}

bool Config::findDevice(const DeviceId& deviceId, ::Server::Config::Device* out) const
{
    return _p->findDevice(deviceId, out);
}

bool Config::findDeviceSource(
    const DeviceId& deviceId,
    const SourceId& sourceId,
    ::Server::Config::Source* source) const
{
    return _p->findDeviceSource(deviceId, sourceId, source);
}

void Config::enumDeviceSources(
    const DeviceId& deviceId,
    const std::function<bool(const Source&)>& callback) const
{
    _p->enumDeviceSources(deviceId, callback);
}

bool Config::findUser(const UserName& userName, ::Server::Config::User* out) const
{
    return _p->findUser(userName, out);
}

bool Config::findUserSource(
    const UserName& userName,
    const SourceId& sourceId,
    PlaySource* out) const
{
    return _p->findUserSource(userName, sourceId, out);
}

}

}
