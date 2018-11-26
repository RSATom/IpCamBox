#include "Log.h"

#include "NetworkCore/Log.h"
#include "ControlServer/Log.h"
#include "RestreamServer/Log.h"


void InitServerLoggers(bool daemon)
{
    NetworkCore::InitLoggers(daemon);
    ControlServer::InitLoggers(daemon);
    RestreamServer::InitLoggers(daemon);
}
