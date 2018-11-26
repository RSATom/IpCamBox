#include "Daemon.h"

#include "Log.h"


bool Daemonize(asio::io_context* ioService)
{
    InitDaemonLoggers("ControlServer");

    auto log = DaemonLog();

    asio::signal_set signals(*ioService, SIGINT, SIGTERM);
    signals.async_wait(
        std::bind(&asio::io_service::stop, ioService));

    if(ioService)
        ioService->notify_fork(asio::io_service::fork_prepare);
    const pid_t firstPid = fork();
    switch(firstPid) {
    case -1:
        log->error("first fork failed: {}", strerror(errno));
        // error
        if(ioService)
            ioService->notify_fork(asio::io_service::fork_parent);
        assert(false);
        return false;
    case 0:
        // child
        if(ioService)
            ioService->notify_fork(asio::io_service::fork_child);
        break;
    default:
        // parent
        if(ioService)
            ioService->notify_fork(asio::io_service::fork_parent);
        return false;
    }

    const pid_t newSessionId = setsid();
    assert(-1 != newSessionId);

    if(-1 == chdir("/"))
        assert(false);

    umask(0027);

    ioService->notify_fork(asio::io_service::fork_prepare);
    const pid_t secondPid = fork();
    switch(secondPid) {
    case -1:
        log->error("second fork failed: {}", strerror(errno));
        // error
        if(ioService)
            ioService->notify_fork(asio::io_service::fork_parent);
        assert(false);
        return false;
    case 0:
        // child
        if(ioService)
            ioService->notify_fork(asio::io_service::fork_child);
        break;
    default:
        // parent
        if(ioService)
            ioService->notify_fork(asio::io_service::fork_parent);
        return false;
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_WRONLY);

    return true;
}
