#pragma once

#include <functional>
#include <unordered_map>
#include <map>

#include <Common/CommonTypes.h>


namespace ControlServer
{

class ServerSession;


class SessionContext
{
public:
    ServerSession* activeSession() const;

    void authenticated(DeviceId, ServerSession*);
    void destroyed(DeviceId);

    void streamRequested(const SourceId& sourceId, const StreamDst&);
    void stopStreamRequested(const SourceId& sourceId);

    bool shouldStream(const SourceId& sourceId, StreamDst* dst = nullptr);
    void enumActiveStreams(
        const std::function<bool (const SourceId& sourceId, const StreamDst& dst)>&);

private:
    ServerSession* _activeSession;
    std::map<SourceId, StreamDst> _activeSources;
};


class Sessions
{
public:
    SessionContext* find(DeviceId);
    SessionContext& get(DeviceId);

private:
    std::unordered_map<DeviceId, SessionContext> _activeSessions;
};

}
