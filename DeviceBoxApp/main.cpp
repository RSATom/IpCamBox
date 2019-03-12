#include <thread>

#include <unistd.h>

#include "Common/Keys.h"

#include "DeviceBox/Log.h"
#include "DeviceBox/DeviceBoxMain.h"


#if !USE_TEST_CLIENT_CERT
static const char* CertificateFile = "./client.crt";
static const char* KeyFile = "./client.key";
#endif

struct FileClose
{
    void operator() (FILE* file)
        { fclose(file); }
};

typedef
    std::unique_ptr<
        FILE,
        FileClose> FILEPtr;

int main(int argc, char* argv[])
{
    DeviceBox::InitDeviceBoxLoggers(false);

    if(argc < 2) {
        DeviceBox::Log()->critical("Missing server host name");
        return -1;
    }

    const std::string serverHost = argv[1];

    DeviceBox::Log()->info("Server host: \"{}\"", serverHost);

#if USE_TEST_CLIENT_CERT
    std::string clientCertificate =
        std::string(TestClientCertificate) + TestClientKey;
#else
    FILEPtr certFilePtr(fopen(CertificateFile, "rb"));
    if(!certFilePtr) {
        DeviceBox::Log()->critical(
            "Fail to open certificate file: {}",
            CertificateFile);
        return -1;
    }

    FILEPtr keyFilePtr(fopen(KeyFile, "rb"));
    if(!keyFilePtr) {
        DeviceBox::Log()->critical(
            "Fail to open key file: {}",
            KeyFile);
        return -1;
    }

    FILE* certFile = certFilePtr.get();
    FILE* keyFile = keyFilePtr.get();

    fseek(certFile , 0 , SEEK_END);
    const long certFileSize = ftell(certFile);
    rewind(certFile);

    fseek(keyFile , 0 , SEEK_END);
    const long keyFileSize = ftell(keyFile);
    rewind(keyFile);

    std::string clientCertificate(certFileSize + keyFileSize, ' ');

    if(certFileSize <= 0 ||
        fread(&clientCertificate[0], 1,
              certFileSize, certFile) != static_cast<size_t>(certFileSize))
    {
        DeviceBox::Log()->critical(
            "Fail to read certificate file: {}",
            CertificateFile);
        return -1;
    }

    if(keyFileSize <= 0 ||
       fread(&clientCertificate[certFileSize], 1,
             keyFileSize, keyFile) != static_cast<size_t>(keyFileSize))
    {
        DeviceBox::Log()->critical("Fail to read key file: {}",
        KeyFile);
        return -1;
    }
#endif

    DeviceBox::AuthConfig authConfig {
        .certificate = clientCertificate,
    };

    asio::io_service ioService;

    DeviceBoxMain(
        &ioService,
        authConfig,
        serverHost,
        DEFAULT_CONTROL_SERVER_PORT,
        false);

    return 0;
}
