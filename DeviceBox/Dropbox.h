#pragma once

#include <string>
#include <deque>

#include <asio.hpp>

#include "Log.h"


namespace DeviceBox
{

struct DropboxInternal;
class Dropbox
{
public:
    Dropbox(asio::io_service*);
    ~Dropbox();

    void setToken(const std::string& token);

    void upload(
        const std::string& src,
        const std::string& dst,
        const std::function<void (long responseCode, const std::string& response)>& finished);
    void listFolder(
        const std::string& path, bool recursive,
        const std::function<void (long responseCode, const std::string& response)>& finished);
    void continueListFolder(
        const std::string& cursor,
        const std::function<void (long responseCode, const std::string& response)>& finished);
    void latestFolderCursor(
        const std::string& path, bool recursive,
        const std::function<void (long responseCode, const std::string& response)>& finished);
    void deletePath(
        const std::string& path,
        const std::function<void (long responseCode, const std::string& response)>& finished);
    void deleteBatch(
        const std::deque<std::string>& ,
        const std::function<void (long responseCode, const std::string& response)>& finished);

    void reset(const std::function<void ()>& finished);
    void shutdown(const std::function<void ()>& finished);

private:
    asio::io_service *const _ioService;

    std::unique_ptr<DropboxInternal> _internal;
};

}
