#pragma once

#include <deque>
#include <unordered_set>

#include <asio.hpp>

#include "Common/RefCounter.h"

#include "Log.h"
#include "Dropbox.h"


namespace DeviceBox
{

class DropboxFolder
{
public:
    DropboxFolder(asio::io_service*, Dropbox*);

    bool active() const;

    void startSync(const std::string& path);
    uint64_t folderSize() const;

    // will delete oldest items
    void shrinkFolder(uint64_t maxSize);

    void shutdown(const std::function<void ()>& finished);

private:
    enum {
        UPDATE_INTERVAL = 5,
    };

    struct Item {
        Item(const std::string& path); // for search purposes only
        Item(std::string&& path, time_t, uint64_t size);
        Item(Item&&);
        void operator = (Item&&);

        std::string path;
        time_t modifiedTimestamp;
        uint64_t size;
    };

    struct ItemPathHash
    {
        size_t operator() (const Item& item) const
            { return hash(item.path); }

        std::hash<std::string> hash;
    };

    struct ItemsLessByTimestamp
    {
        bool operator() (const Item& x, const Item& y) const
            { return x.modifiedTimestamp < y.modifiedTimestamp; }
        bool operator() (const Item* x, const Item* y) const
            { return x->modifiedTimestamp < y->modifiedTimestamp; }
        bool operator() (time_t t, const Item* y) const
            { return t < y->modifiedTimestamp; }
        bool operator() (const Item* x, time_t t) const
            { return x->modifiedTimestamp < t; }
    };

    struct ItemsEqualByPath
    {
        bool operator() (const Item& x, const Item& y) const
            { return x.path == y.path; }
        bool operator() (const Item* x, const Item* y) const
            { return x->path == y->path; }
    };

    struct ListFolderJsonHandler;

    static inline const std::shared_ptr<spdlog::logger>& Log();

    void onListFolderResponse(long responseCode, const std::string& response);

    void update(const std::string& cursor);
    void onUpdateResponse(long responseCode, const std::string& response);

    void handleFolderResponse(const std::string& response);

    void eraseItem(const Item&);
    void eraseItem(const std::string& path);

private:
    RefCounter<DropboxFolder> _thisRefCounter;
    bool _shuttingDown;

    asio::io_service* _ioService;
    Dropbox* _dropbox;
    std::deque<const Item*> _index; // ordered by modifiedTimestamp
    std::unordered_set<Item, ItemPathHash, ItemsEqualByPath> _items;

    asio::steady_timer _updateTimer;

    uint64_t _folderSize;
};

}
