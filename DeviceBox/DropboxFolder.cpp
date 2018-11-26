#include "DropboxFolder.h"

#include <time.h>

#include <rapidjson/document.h>


namespace DeviceBox
{

///////////////////////////////////////////////////////////////////////////////
DropboxFolder::Item::Item(const std::string& path) :
    path(path), modifiedTimestamp(0), size(0)
{
}

DropboxFolder::Item::Item(std::string&& path, time_t modified, uint64_t size) :
    path(path), modifiedTimestamp(modified), size(size)
{
}

DropboxFolder::Item::Item(Item&& item) :
    path(std::move(item.path)), modifiedTimestamp(item.modifiedTimestamp), size(item.size)
{
}

void DropboxFolder::Item::operator = (Item&& item)
{
    path.swap(item.path);
    modifiedTimestamp = item.modifiedTimestamp;
    size = item.size;
}


///////////////////////////////////////////////////////////////////////////////
const std::shared_ptr<spdlog::logger>& DropboxFolder::Log()
{
    return DropboxLog();
}

DropboxFolder::DropboxFolder(asio::io_service* ioService, Dropbox* dropbox) :
    _thisRefCounter(this),_shuttingDown(false),
    _ioService(ioService), _dropbox(dropbox), _updateTimer(*ioService), _folderSize(0)
{
}

uint64_t DropboxFolder::folderSize() const
{
    return _folderSize;
}

void DropboxFolder::startSync(const std::string& path)
{
    Log()->debug("Start sync \"{}\"", path);

    assert(!_shuttingDown);

    _dropbox->listFolder(
        path, true,
        std::bind(
            &DropboxFolder::onListFolderResponse, _thisRefCounter,
            std::placeholders::_1, std::placeholders::_2));
}

void DropboxFolder::onListFolderResponse(long responseCode, const std::string& response)
{
    if(200 != responseCode) {
        Log()->error("List folder failed. Code: {}, Responce: {}", responseCode, response);
        return;
    }

    if(_shuttingDown)
        return;

    handleFolderResponse(response);
}

void DropboxFolder::update(const std::string& cursor)
{
    if(_shuttingDown)
        return;

    _dropbox->continueListFolder(
        cursor,
        std::bind(
            &DropboxFolder::onUpdateResponse, _thisRefCounter,
            std::placeholders::_1, std::placeholders::_2));
}

void DropboxFolder::onUpdateResponse(long responseCode, const std::string& response)
{
    assert(200 == responseCode);

    if(200 != responseCode)
        return;

    if(_shuttingDown)
        return;

    handleFolderResponse(response);
}

void DropboxFolder::handleFolderResponse(const std::string& response)
{
    assert(!_shuttingDown);

    rapidjson::Document doc;

    doc.Parse(response.data(), response.size());

    const auto& entries = doc["entries"].GetArray();
    for(const auto& entry: entries) {
        if(0 == strcmp(entry[".tag"].GetString(), "file")){
            std::string modifiedStr = entry["server_modified"].GetString();
            tm modifiedTm;
            strptime(modifiedStr.c_str(), "%Y-%m-%dT%H:%M:%SZ", &modifiedTm);
            const time_t modifiedTimestamp = mktime(&modifiedTm);

            std::string path = entry["path_display"].GetString();
            const uint64_t size = entry["size"].GetUint64();

            Item newItem(std::move(path), modifiedTimestamp, size);

            eraseItem(newItem);

            const Item* inserted = &(*_items.emplace(std::move(newItem)).first);

            auto indexInsertIt =
                std::upper_bound(
                    _index.begin(), _index.end(), inserted,
                    ItemsLessByTimestamp());

            _index.insert(indexInsertIt, inserted);

            _folderSize += size;
        } else if(0 == strcmp(entry[".tag"].GetString(), "folder")) {
        } else if(0 == strcmp(entry[".tag"].GetString(), "deleted")) {
            const std::string path = entry["path_display"].GetString();
            eraseItem(path);
        }
    }
    assert(std::is_sorted(_index.begin(), _index.end(), ItemsLessByTimestamp()));
    assert(_items.size() == _index.size());

    const std::string cursor =  doc["cursor"].GetString();
    const bool hasMore = doc["has_more"].GetBool();
    if(hasMore)
        update(cursor);
    else {
        auto thisRefCounter(_thisRefCounter);
        _updateTimer.expires_from_now(std::chrono::seconds(UPDATE_INTERVAL));
        _updateTimer.async_wait(
            [this, cursor, thisRefCounter] (const asio::error_code& error) {
                if(error)
                    return;

                update(cursor);
            }
        );
    }
}

void DropboxFolder::eraseItem(const std::string& path)
{
    return eraseItem(Item(path));
}

void DropboxFolder::eraseItem(const Item& item)
{
    assert(!_shuttingDown);

    auto it = _items.find(item);
    if(_items.end() != it) {
        _folderSize -= it->size;

        const time_t oldTimestamp = it->modifiedTimestamp;

        auto lowerIt =
            std::lower_bound(
                _index.begin(), _index.end(), oldTimestamp,
                ItemsLessByTimestamp()
        );
        auto upperIt =
            std::upper_bound(
                lowerIt, _index.end(), oldTimestamp,
                ItemsLessByTimestamp()
        );
        auto oldIt =
            std::find_if(lowerIt, upperIt,
                [&item] (const Item* x) {
                    return x->path == item.path;
                }
            );
        _index.erase(oldIt);

        _items.erase(it);

    }
}

void DropboxFolder::shrinkFolder(uint64_t maxSize)
{
    if(_shuttingDown || maxSize > folderSize())
        return;

    const auto curSize = folderSize();
    uint64_t removeSize = curSize - maxSize;

    std::deque<std::string> removeList;
    for(const Item* item: _index) {
        removeList.push_back(item->path);
        if(removeSize < item->size) {
            removeSize = 0;
            break;
        }
        removeSize -= item->size;
    }

    assert(!removeList.empty());
    if(removeList.empty())
        return;

    auto thisRefCounter(_thisRefCounter);

    _dropbox->deleteBatch(
        removeList,
        [thisRefCounter] (long responseCode, const std::string& response) {
        }
    );
}

bool DropboxFolder::active() const
{
    return _thisRefCounter.hasRefs();
}

void DropboxFolder::shutdown(const std::function<void ()>& finished)
{
    _shuttingDown = true;

    _ioService->post(finished);
}

}
