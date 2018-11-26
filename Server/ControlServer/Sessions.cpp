#include "Sessions.h"

#include "Log.h"


namespace ControlServer
{

ServerSession* SessionContext::activeSession() const
{
    return _activeSession;
}

void SessionContext::authenticated(DeviceId id, ServerSession* session)
{
    Log()->info("Device \"{}\" connected", id);

    assert(!_activeSession);

    _activeSession = session;
}

void SessionContext::destroyed(DeviceId id)
{
    assert(_activeSession);

    _activeSession = nullptr;

    Log()->info(
        "Device \"{}\" disconnected. Active sources count: {}",
        id, _activeSources.size());
}

void SessionContext::streamRequested(const SourceId& sourceId, const StreamDst& dst)
{
    Log()->trace(">> SessionContext::streamRequested, sourceId: {}, destination: {}", sourceId, dst);

    auto it = _activeSources.find(sourceId);
    if(_activeSources.end() == it) {
        _activeSources.insert({sourceId, dst});
    } else {
        assert(false);
        Log()->error(
            "Requested streaming of already active source,"
            "sourceid: {}, active destination: {}, new destination {}",
            sourceId, it->second, dst);
    }
}

void SessionContext::stopStreamRequested(const SourceId& sourceId)
{
    Log()->trace(">> SessionContext::stopStreamRequested, sourceId: {}", sourceId);

    _activeSources.erase(sourceId);
}

bool SessionContext::shouldStream(const SourceId& sourceId, StreamDst* dst /*= nullptr*/)
{
    auto it = _activeSources.find(sourceId);
    if(_activeSources.end() == it) {
        return false;
    } else {
        if(dst)
            *dst = it->second;
        return true;
    }
}

void SessionContext::enumActiveStreams(
    const std::function<bool (const SourceId& sourceId, const StreamDst& dst)>& cb)
{
    for(auto& pair: _activeSources) {
        if(!cb(pair.first, pair.second))
            break;
    }
}


SessionContext* Sessions::find(DeviceId id)
{
    auto it = _activeSessions.find(id);
    if(_activeSessions.end() == it)
        return nullptr;

    return &(it->second);
}

SessionContext& Sessions::get(DeviceId id)
{
    return _activeSessions[id];
}

}
