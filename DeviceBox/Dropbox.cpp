#include "Dropbox.h"

#include <string>
#include <functional>
#include <map>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib.h>

#include <curl/curl.h>

#include "string_format.h"
#include "finally_execute.h"


namespace DeviceBox
{

///////////////////////////////////////////////////////////////////////////////
class DropboxInternal
{
public:
    DropboxInternal(asio::io_service* ioService);
    ~DropboxInternal();

    void setToken(const std::string&);

    void postUpload(
        const std::string& src,
        const std::string& dst,
        const std::function<void (long responseCode, const std::string& response)>& finished);
    void postListFolder(
        const std::string& path, bool recursive,
        const std::function<void (long responseCode, const std::string& response)>& finished);
    void postContinueListFolder(
        const std::string& cursor,
        const std::function<void (long responseCode, const std::string& response)>& finished);
    void postLatestFolderCursor(
        const std::string& path, bool recursive,
        const std::function<void (long responseCode, const std::string& response)>& finished);
    void postDeletePath(
        const std::string& path,
        const std::function<void (long responseCode, const std::string& response)>& finished);
    void postDeleteBatch(
        const std::deque<std::string>& list,
        const std::function<void (long responseCode, const std::string& response)>& finished);

    void postShutdown(const std::function<void ()>& finished);

private:
    enum {
        MAX_UPLOADS = 2,
    };

    struct Action;
    struct UploadAction;
    struct ListFolderAction;
    struct ContinueListFolderAction;
    struct LatestFolderCursorAction;
    struct DeletePathAction;
    struct DeleteBatchAction;

    static inline const std::shared_ptr<spdlog::logger>& Log();

    void curlThreadMain();
    void curlPerform();
    void handleDone(CURL* curl);

    void actionFinished(
        const std::function<void (long responseCode, const std::string& response)>& finished,
        bool internal,
        long responseCode, const std::string& response);

    void upload(
        const std::string& src,
        const std::string& dst,
        const std::function<void (long responseCode, const std::string& response)>& finished);
    void uploadFinished(
            const std::function<void (long responseCode, const std::string& response)>& finished,
            long responseCode, const std::string& response);

    void listFolder(
        const std::string& path, bool recursive,
        const std::function<void (long responseCode, const std::string& response)>& finished,
        bool internal);
    void continueListFolder(
        const std::string& cursor,
        const std::function<void (long responseCode, const std::string& response)>& finished,
        bool internal);
    void latestFolderCursor(
        const std::string& path, bool recursive,
        const std::function<void (long responseCode, const std::string& response)>& finished,
        bool internal);
    void deletePath(
        const std::string& path,
        const std::function<void (long responseCode, const std::string& response)>& finished,
        bool internal);
    void deleteBatch(
        const std::deque<std::string>& list,
        const std::function<void (long responseCode, const std::string& response)>& finished,
        bool internal);

    void shutdown(const std::function<void ()>& finished);

private:
    asio::io_service* _ioService;

    std::string _token;

    asio::io_service _curlIoService;
    std::unique_ptr<asio::io_service::work> _working;

    std::thread _curlThread;
    CURLM* _curlMulti;
    int _runningCount;
    asio::steady_timer _timeoutTimer;

    std::map<CURL*, std::shared_ptr<Action> > _actions;

    unsigned _uploadCount;

    std::function<void ()> _shutdowned;
};


///////////////////////////////////////////////////////////////////////////////
struct DropboxInternal::Action
{
    Action(bool internal);
    ~Action();

    virtual void handleDone(DropboxInternal* parent);

protected:
    bool isInternal() const;

    CURL* init(const std::string& token);
    curl_slist*& headers();

    const std::string& response() const;
    long responseCode() const;

private:
    static size_t collectResponse(char* ptr, size_t size, size_t nmemb, Action* userdata);

private:
    const bool _internal;

    CURL* _curl;
    curl_slist* _headers;
    std::string _response;
    long _responseCode;
};


///////////////////////////////////////////////////////////////////////////////
class DropboxInternal::UploadAction : public DropboxInternal::Action
{
public:
    UploadAction(const std::function<void (long responseCode, const std::string& response)>& finished);
    ~UploadAction();

    CURL* init(
        const std::string& token,
        const std::string& dst,
        const std::string& src);

    void handleDone(DropboxInternal* parent) override;

private:
    std::function<void (long responseCode, const std::string& response)> _finished;

    std::string _destination;
    std::string _source;

    FILE* _file;
};


///////////////////////////////////////////////////////////////////////////////
class DropboxInternal::ListFolderAction : public DropboxInternal::Action
{
public:
    ListFolderAction(
        const std::function<void (long responseCode, const std::string& response)>& finished,
        bool internal);

    CURL* init(
        const std::string& token,
        const std::string& path,
        bool recursive);

    void handleDone(DropboxInternal* parent) override;

private:
    std::string _path;

    std::string _data;
    std::function<void (long responseCode, const std::string& response)> _finished;
};


///////////////////////////////////////////////////////////////////////////////
class DropboxInternal::ContinueListFolderAction : public DropboxInternal::Action
{
public:
    ContinueListFolderAction(
        const std::function<void (long responseCode, const std::string& response)>& finished,
        bool internal);

    CURL* init(
        const std::string& token,
        const std::string& cursor);

    void handleDone(DropboxInternal* parent) override;

private:
    std::string _data;
    std::function<void (long responseCode, const std::string& response)> _finished;
};


///////////////////////////////////////////////////////////////////////////////
class DropboxInternal::LatestFolderCursorAction : public DropboxInternal::Action
{
public:
    LatestFolderCursorAction(
        const std::function<void (long responseCode, const std::string& response)>& finished,
        bool internal);

    CURL* init(
        const std::string& token,
        const std::string& path,
        bool recursive);

    void handleDone(DropboxInternal* parent) override;

private:
    std::string _data;
    std::function<void (long responseCode, const std::string& response)> _finished;
};


///////////////////////////////////////////////////////////////////////////////
class DropboxInternal::DeletePathAction : public DropboxInternal::Action
{
public:
    DeletePathAction(
        const std::function<void (long responseCode, const std::string& response)>& finished,
        bool internal);

    CURL* init(
        const std::string& token,
        const std::string& path);

    void handleDone(DropboxInternal* parent) override;

private:
    std::string _data;
    std::function<void (long responseCode, const std::string& response)> _finished;
};


///////////////////////////////////////////////////////////////////////////////
class DropboxInternal::DeleteBatchAction : public DropboxInternal::Action
{
public:
    DeleteBatchAction(
        const std::function<void (long responseCode, const std::string& response)>& finished,
        bool internal);

    CURL* init(
        const std::string& token,
        const std::deque<std::string>& list);

    void handleDone(DropboxInternal* parent) override;

private:
    std::string _data;
    std::function<void (long responseCode, const std::string& response)> _finished;
};


///////////////////////////////////////////////////////////////////////////////
const std::shared_ptr<spdlog::logger>& DropboxInternal::Log()
{
    return DropboxLog();
}

DropboxInternal::DropboxInternal(asio::io_service* ioService) :
    _ioService(ioService),
    _curlMulti(nullptr), _runningCount(0),
    _timeoutTimer(_curlIoService), _uploadCount(0)
{
    curl_global_init(CURL_GLOBAL_ALL); // FIXME!
    _curlThread = std::thread(&DropboxInternal::curlThreadMain, this);
}

DropboxInternal::~DropboxInternal()
{
    _timeoutTimer.cancel();

    assert(!_curlThread.joinable());

    curl_global_cleanup(); // FIXME!
}

void DropboxInternal::setToken(const std::string& token)
{
    _token = token;
}

void DropboxInternal::curlThreadMain()
{
    _working.reset(new asio::io_service::work(_curlIoService));

    _curlMulti = curl_multi_init();

    _curlIoService.run();

    curl_multi_cleanup(_curlMulti);
    _curlMulti = nullptr;

    _ioService->post(
        [this] () {
            _ioService->post(_shutdowned);
            _curlThread.join();
        }
    );
}

void DropboxInternal::handleDone(CURL* curl)
{
    CURLMcode code = curl_multi_remove_handle(_curlMulti, curl);
    // FIXME! обработка ошибки?
    assert(code == CURLM_OK);

    auto it = _actions.find(curl);
    assert(it != _actions.end());
    if(it == _actions.end()) {
        Log()->error("Action not found in DropboxInternal::handleDone");
        return;
    }

    it->second->handleDone(this);

    _actions.erase(it);
}

void DropboxInternal::curlPerform()
{
    const int prevRunningCount = _runningCount;
    CURLMcode code = curl_multi_perform(_curlMulti, &_runningCount);
    // FIXME! обработка ошибки?
    assert(code == CURLM_OK);

    if(prevRunningCount != _runningCount) {
        CURLMsg* message;
        do {
            int messagesLeft = 0;
            message = curl_multi_info_read(_curlMulti, &messagesLeft);

            if(message && (message->msg == CURLMSG_DONE))
                handleDone(message->easy_handle);
        } while(message);
    }

    long milliseconds;
    code = curl_multi_timeout(_curlMulti, &milliseconds);
    // FIXME! обработка ошибки?
    assert(code == CURLM_OK);

    _timeoutTimer.cancel();
    if(0 == milliseconds)
        _curlIoService.post(std::bind(&DropboxInternal::curlPerform, this));
    else {
        if(milliseconds < 0)
            milliseconds = 10; // FIXME

        _timeoutTimer.expires_from_now(std::chrono::milliseconds(milliseconds));
        _timeoutTimer.async_wait(
            [this] (const asio::error_code& error) {
                if(error)
                    return;

                curlPerform();
            }
        );
    }
}

void DropboxInternal::actionFinished(
    const std::function<void (long responseCode, const std::string& response)>& finished,
    bool internal,
    long responseCode, const std::string& response)
{
    if(internal)
        finished(responseCode, response);
    else
        _ioService->post(std::bind(finished, responseCode, response));
}

void DropboxInternal::uploadFinished(
    const std::function<void (long responseCode, const std::string& response)>& finished,
    long responseCode, const std::string& response)
{
    --_uploadCount;

    actionFinished(finished, false, responseCode, response);
}

void DropboxInternal::upload(
    const std::string& src,
    const std::string& dst,
    const std::function<void (long responseCode, const std::string& response)>& finished)
{
    ++_uploadCount;

    if(_uploadCount > MAX_UPLOADS) {
        uploadFinished(finished, 0, std::string());
        Log()->debug("Too many simultaneous uploads. Skipped: src: {}, dst: {}", src, dst);
        return;
    }

    Log()->debug("Upload: src: {}, dst: {}", src, dst);

    std::shared_ptr<UploadAction> upload = std::make_shared<UploadAction>(finished);
    CURL* curl =
        upload->init(_token, dst, src);

    if(!curl) {
        upload->handleDone(this);
        return;
    }

    _actions.emplace(curl, upload);

    curl_multi_add_handle(_curlMulti, curl);

    curlPerform();
}

void DropboxInternal::listFolder(
    const std::string& path, bool recursive,
    const std::function<void (long responseCode, const std::string& response)>& finished,
    bool internal)
{
    std::shared_ptr<ListFolderAction> listFolder =
        std::make_shared<ListFolderAction>(finished, internal);
    CURL* curl =
        listFolder->init(_token, path, recursive);

    if(!curl) {
        listFolder->handleDone(this);
        return;
    }

    _actions.emplace(curl, listFolder);

    curl_multi_add_handle(_curlMulti, curl);

    curlPerform();
}

void DropboxInternal::continueListFolder(
    const std::string& cursor,
    const std::function<void (long responseCode, const std::string& response)>& finished,
    bool internal)
{
    std::shared_ptr<ContinueListFolderAction> continueListFolderAction =
        std::make_shared<ContinueListFolderAction>(finished, internal);
    CURL* curl =
        continueListFolderAction->init(_token, cursor);

    if(!curl) {
        continueListFolderAction->handleDone(this);
        return;
    }

    _actions.emplace(curl, continueListFolderAction);

    curl_multi_add_handle(_curlMulti, curl);

    curlPerform();
}

void DropboxInternal::latestFolderCursor(
    const std::string& path, bool recursive,
    const std::function<void (long responseCode, const std::string& response)>& finished,
    bool internal)
{
    std::shared_ptr<LatestFolderCursorAction> latestFolderCursor =
        std::make_shared<LatestFolderCursorAction>(finished, internal);
    CURL* curl =
        latestFolderCursor->init(_token, path, recursive);

    if(!curl) {
        latestFolderCursor->handleDone(this);
        return;
    }

    _actions.emplace(curl, latestFolderCursor);

    curl_multi_add_handle(_curlMulti, curl);

    curlPerform();
}

void DropboxInternal::deletePath(
    const std::string& path,
    const std::function<void (long responseCode, const std::string& response)>& finished,
    bool internal)
{
    std::shared_ptr<DeletePathAction> action =
        std::make_shared<DeletePathAction>(finished, internal);
    CURL* curl =
        action->init(_token, path);

    if(!curl) {
        action->handleDone(this);
        return;
    }

    _actions.emplace(curl, action);

    curl_multi_add_handle(_curlMulti, curl);

    curlPerform();
}

void DropboxInternal::deleteBatch(
    const std::deque<std::string>& list,
    const std::function<void (long responseCode, const std::string& response)>& finished,
    bool internal)
{
    std::shared_ptr<DeleteBatchAction> action =
        std::make_shared<DeleteBatchAction>(finished, internal);
    CURL* curl =
        action->init(_token, list);

    if(!curl) {
        action->handleDone(this);
        return;
    }

    _actions.emplace(curl, action);

    curl_multi_add_handle(_curlMulti, curl);

    curlPerform();
}

void DropboxInternal::shutdown(const std::function<void ()>& finished)
{
    _shutdowned = finished;
    _working.reset();
}

void DropboxInternal::postUpload(
    const std::string& dst,
    const std::string& fileName,
    const std::function<void (long responseCode, const std::string& response)>& finished)
{
    _curlIoService.post(
        std::bind(&DropboxInternal::upload, this, dst, fileName, finished));
}

void DropboxInternal::postListFolder(
    const std::string& path, bool recursive,
    const std::function<void (long responseCode, const std::string& response)>& finished)
{
    _curlIoService.post(
        std::bind(&DropboxInternal::listFolder, this, path, recursive, finished, false));
}

void DropboxInternal::postContinueListFolder(
    const std::string& cursor,
    const std::function<void (long responseCode, const std::string& response)>& finished)
{
    _curlIoService.post(
        std::bind(&DropboxInternal::continueListFolder, this, cursor, finished, false));
}

void DropboxInternal::postLatestFolderCursor(
    const std::string& path, bool recursive,
    const std::function<void (long responseCode, const std::string& response)>& finished)
{
    _curlIoService.post(
        std::bind(&DropboxInternal::latestFolderCursor, this, path, recursive, finished, false));
}

void DropboxInternal::postDeletePath(
    const std::string& path,
    const std::function<void (long responseCode, const std::string& response)>& finished)
{
    _curlIoService.post(
        std::bind(&DropboxInternal::deletePath, this, path, finished, false));
}

void DropboxInternal::postDeleteBatch(
    const std::deque<std::string>& list,
    const std::function<void (long responseCode, const std::string& response)>& finished)
{
    _curlIoService.post(
        std::bind(&DropboxInternal::deleteBatch, this, list, finished, false));
}

void DropboxInternal::postShutdown(const std::function<void ()>& finished)
{
    _curlIoService.post(
        std::bind(&DropboxInternal::shutdown, this, finished));
}


///////////////////////////////////////////////////////////////////////////////
DropboxInternal::Action::Action(bool internal) :
    _internal(internal), _curl(nullptr), _headers(nullptr), _responseCode(0)
{
    _curl = curl_easy_init();
}

DropboxInternal::Action::~Action()
{
    curl_easy_cleanup(_curl);
    _curl = nullptr;

    curl_slist_free_all(_headers);
    _headers = nullptr;
}

size_t DropboxInternal::Action::collectResponse(char* ptr, size_t size, size_t nmemb, Action* self)
{
    self->_response.append(ptr, size * nmemb);
    return size * nmemb;
}

bool DropboxInternal::Action::isInternal() const
{
    return _internal;
}

CURL* DropboxInternal::Action::init(const std::string& token)
{
    if(!_curl)
        return nullptr; // FIXME!

    const char* Auth = "Authorization: Bearer %_";

    _headers = curl_slist_append(nullptr, string_format(Auth)(token).str().c_str());

    curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, collectResponse);
    curl_easy_setopt(_curl, CURLOPT_WRITEDATA, this);

    return _curl;
}

curl_slist*& DropboxInternal::Action::headers()
{
    return _headers;
}

void DropboxInternal::Action::handleDone(DropboxInternal*)
{
    curl_easy_getinfo(_curl, CURLINFO_RESPONSE_CODE, &_responseCode);
}

const std::string& DropboxInternal::Action::response() const
{
    return _response;
}

long DropboxInternal::Action::responseCode() const
{
    return _responseCode;
}


///////////////////////////////////////////////////////////////////////////////
DropboxInternal::UploadAction::UploadAction(
    const std::function<void (long responseCode, const std::string& response)>& finished) :
    Action(false), _finished(finished), _file(nullptr)
{
}

DropboxInternal::UploadAction::~UploadAction()
{
    if(_file)
        fclose(_file);
}

CURL* DropboxInternal::UploadAction::init(
    const std::string& token,
    const std::string& dst,
    const std::string& src)
{
    _destination = dst;
    _source = src;

    CURL* curl = Action::init(token);
    if(!curl)
        return nullptr;

    const char* ApiUrl = "https://content.dropboxapi.com/2/files/upload";

    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, ApiUrl);

    const char* ApiArg =
        "Dropbox-API-Arg: { "
            "\"path\": \"%_\", "
            "\"mode\": \"overwrite\", "
            "\"autorename\": false, "
            "\"mute\": false "
        "}";

    _file = fopen(src.c_str(), "rb");
    if(!_file)
        return nullptr;

    headers() = curl_slist_append(headers(), string_format(ApiArg)(dst).str().c_str());
    headers() = curl_slist_append(headers(), "Content-Type: application/octet-stream");
    headers() = curl_slist_append(headers(), "Transfer-Encoding: chunked");
    headers() = curl_slist_append(headers(), "Expect:");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers());

    curl_easy_setopt(curl, CURLOPT_READDATA, _file);

    return curl;
}

void DropboxInternal::UploadAction::handleDone(DropboxInternal* owner)
{
    Action::handleDone(owner);

    owner->Log()->debug(
        "Upload finished: dst: {}, response: {}",
        _destination, responseCode());

    owner->uploadFinished(_finished, responseCode(), response());
}


///////////////////////////////////////////////////////////////////////////////
DropboxInternal::ListFolderAction::ListFolderAction(
    const std::function<void (long responseCode, const std::string& response)>& finished,
    bool internal) :
    Action(internal), _finished(finished)
{
}

CURL* DropboxInternal::ListFolderAction::init(
    const std::string& token,
    const std::string& path,
    bool recursive)
{
    _path = path;

    CURL* curl = Action::init(token);
    if(!curl)
        return nullptr;

    const char* ApiUrl = "https://api.dropboxapi.com/2/files/list_folder";

    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, ApiUrl);

    headers() = curl_slist_append(headers(), "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers());

    const char* Data =
        "{ "
            "\"path\": \"%_\", "
            "\"recursive\": %_,"
            "\"include_media_info\": false,"
            "\"include_deleted\": false,"
            "\"include_has_explicit_shared_members\": false"
        "}";

    _data = string_format(Data)(path)(recursive ? "true" : "false").str();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, _data.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, _data.size());

    return curl;
}

void DropboxInternal::ListFolderAction::handleDone(DropboxInternal* owner)
{
    Action::handleDone(owner);

    owner->Log()->debug(
        "List folder finished: path: {}, code: {}",
        _path, responseCode());

    owner->actionFinished(_finished, isInternal(), responseCode(), response());
}


///////////////////////////////////////////////////////////////////////////////
DropboxInternal::ContinueListFolderAction::ContinueListFolderAction(
    const std::function<void (long responseCode, const std::string& response)>& finished,
    bool internal) :
    Action(internal), _finished(finished)
{
}

CURL* DropboxInternal::ContinueListFolderAction::init(
    const std::string& token,
    const std::string& cursor)
{
    CURL* curl = Action::init(token);
    if(!curl)
        return nullptr;

    const char* ApiUrl = "https://api.dropboxapi.com/2/files/list_folder/continue";

    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, ApiUrl);

    headers() = curl_slist_append(headers(), "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers());

    const char* Data =
        "{ "
            "\"cursor\": \"%_\" "
        "}";

    _data = string_format(Data)(cursor).str();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, _data.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, _data.size());

    return curl;
}

void DropboxInternal::ContinueListFolderAction::handleDone(DropboxInternal* owner)
{
    Action::handleDone(owner);

    owner->Log()->debug(
        "Continue list folder finished: response: {}",
        responseCode());

    owner->actionFinished(_finished, isInternal(), responseCode(), response());
}


///////////////////////////////////////////////////////////////////////////////
DropboxInternal::LatestFolderCursorAction::LatestFolderCursorAction(
    const std::function<void (long responseCode, const std::string& response)>& finished,
    bool internal) :
    Action(internal), _finished(finished)
{
}

CURL* DropboxInternal::LatestFolderCursorAction::init(
    const std::string& token,
    const std::string& path,
    bool recursive)
{
    CURL* curl = Action::init(token);
    if(!curl)
        return nullptr;

    const char* ApiUrl = "https://api.dropboxapi.com/2/files/list_folder/get_latest_cursor";

    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, ApiUrl);

    headers() = curl_slist_append(headers(), "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers());

    const char* Data =
        "{ "
            "\"path\": \"%_\", "
            "\"recursive\": %_,"
            "\"include_media_info\": false,"
            "\"include_deleted\": false,"
            "\"include_has_explicit_shared_members\": false"
        "}";

    _data = string_format(Data)(path)(recursive ? "true" : "false").str();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, _data.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, _data.size());

    return curl;
}

void DropboxInternal::LatestFolderCursorAction::handleDone(DropboxInternal* owner)
{
    Action::handleDone(owner);

    owner->Log()->debug(
        "Latest folder cursor finished: response: {}",
        responseCode());

    owner->actionFinished(_finished, isInternal(), responseCode(), response());
}


///////////////////////////////////////////////////////////////////////////////
DropboxInternal::DeletePathAction::DeletePathAction(
    const std::function<void (long responseCode, const std::string& response)>& finished,
    bool internal) :
    Action(internal), _finished(finished)
{
}

CURL* DropboxInternal::DeletePathAction::init(
    const std::string& token,
    const std::string& path)
{
    CURL* curl = Action::init(token);
    if(!curl)
        return nullptr;

    const char* ApiUrl = "https://api.dropboxapi.com/2/files/delete";

    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, ApiUrl);

    headers() = curl_slist_append(headers(), "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers());

    const char* Data =
        "{ "
            "\"path\": \"%_\" "
        "}";

    _data = string_format(Data)(path).str();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, _data.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, _data.size());

    return curl;
}

void DropboxInternal::DeletePathAction::handleDone(DropboxInternal* owner)
{
    Action::handleDone(owner);

    owner->Log()->debug(
        "Delete path finished: response: {}",
        responseCode());

    owner->actionFinished(_finished, isInternal(), responseCode(), response());
}


///////////////////////////////////////////////////////////////////////////////
DropboxInternal::DeleteBatchAction::DeleteBatchAction(
    const std::function<void (long responseCode, const std::string& response)>& finished,
    bool internal) :
    Action(internal), _finished(finished)
{
}

CURL* DropboxInternal::DeleteBatchAction::init(
    const std::string& token,
    const std::deque<std::string>& list)
{
    CURL* curl = Action::init(token);
    if(!curl)
        return nullptr;

    const char* ApiUrl = "https://api.dropboxapi.com/2/files/delete_batch";

    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, ApiUrl);

    headers() = curl_slist_append(headers(), "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers());

    std::ostringstream dataStream;
    dataStream <<
        "{ "
            "\"entries\": [";

    bool first = true;
    for(const std::string& path: list) {
        if(first)
            first = false;
        else
            dataStream << ", ";

        dataStream << "{ \"path\": \"";
        dataStream << path;
        dataStream << "\" }";
    }

    dataStream <<
            "]"
        "}";

    _data = dataStream.str();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, _data.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, _data.size());

    return curl;
}

void DropboxInternal::DeleteBatchAction::handleDone(DropboxInternal* owner)
{
    Action::handleDone(owner);

    owner->Log()->debug(
        "Delete Batch finished: response: {}",
        responseCode());

    owner->actionFinished(_finished, isInternal(), responseCode(), response());
}


///////////////////////////////////////////////////////////////////////////////
Dropbox::Dropbox(asio::io_service* io_service) :
    _ioService(io_service), _internal(new DropboxInternal(io_service))
{
}

Dropbox::~Dropbox()
{
}

void Dropbox::setToken(const std::string& token)
{
    if(!_internal) {
        assert(false);
        return;
    }
    _internal->setToken(token);
}

void Dropbox::upload(
    const std::string& src,
    const std::string& dst,
    const std::function<void (long responseCode, const std::string& response)>& finished)
{
    if(!_internal) {
        assert(false);
        return;
    }
    _internal->postUpload(src, dst, finished);
}

void Dropbox::listFolder(
    const std::string& path, bool recursive,
    const std::function<void (long responseCode, const std::string& response)>& finished)
{
    if(!_internal) {
        assert(false);
        return;
    }
    _internal->postListFolder(path, recursive, finished);
}

void Dropbox::continueListFolder(
    const std::string& cursor,
    const std::function<void (long responseCode, const std::string& response)>& finished)
{
    if(!_internal) {
        assert(false);
        return;
    }
    _internal->postContinueListFolder(cursor, finished);
}

void Dropbox::latestFolderCursor(
    const std::string& path, bool recursive,
    const std::function<void (long responseCode, const std::string& response)>& finished)
{
    if(!_internal) {
        assert(false);
        return;
    }
    _internal->postLatestFolderCursor(path, recursive, finished);
}

void Dropbox::deletePath(
    const std::string& path,
    const std::function<void (long responseCode, const std::string& response)>& finished)
{
    if(!_internal) {
        assert(false);
        return;
    }
    _internal->postDeletePath(path, finished);
}

void Dropbox::deleteBatch(
    const std::deque<std::string>& list,
    const std::function<void (long responseCode, const std::string& response)>& finished)
{
    if(!_internal) {
        assert(false);
        return;
    }
    _internal->postDeleteBatch(list, finished);
}

void Dropbox::reset(const std::function<void ()>& finished)
{
    if(!_internal) {
        assert(false);
        _ioService->post(finished);
        return;
    }

    auto recreateInternal =
        [this, finished] () {
            _internal.reset(new DropboxInternal(_ioService));
            _ioService->post(finished);
        };

    _internal->postShutdown(recreateInternal);
}

void Dropbox::shutdown(const std::function<void ()>& finished)
{
    if(!_internal) {
        assert(false);
        _ioService->post(finished);
        return;
    }

    auto resetInternal =
        [this, finished] () {
            _internal.reset();
            _ioService->post(finished);
        };

    _internal->postShutdown(resetInternal);
}

}
