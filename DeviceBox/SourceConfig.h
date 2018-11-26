#pragma once

#include <string>


namespace DeviceBox
{

struct SourceConfig
{
    std::string id;
    std::string uri;

    std::string user;
    std::string password;

    std::string archivePath;
    unsigned desiredFileSize;

    std::string dropboxArchivePath;
    uint64_t dropboxMaxStorage;
};

}
