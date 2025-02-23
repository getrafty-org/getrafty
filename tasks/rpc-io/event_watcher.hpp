#pragma once

#include <map>
#include <sys/epoll.h>
#include <shared_mutex>
#include "thread_pool.hpp"


namespace getrafty::rpc::io {

using wheels::concurrent::ThreadPool;
using EpollWaitFunc = std::function<int(int, epoll_event *, int, int)>;

enum WatchFlag : uint8_t {
    CB_RDONLY = 0x00,
    CB_WRONLY = 0x01,
};

class IWatchCallback {
public:
    virtual void onReadReady(int /*fd*/) {}
    virtual void onWriteReady(int /*fd*/){}
    virtual ~IWatchCallback() = default;
};

class EventWatcher {
public:
    explicit EventWatcher(EpollWaitFunc epoll_impl = ::epoll_wait );
    ~EventWatcher();

    void watch(int fd, WatchFlag flag, IWatchCallback *ch);
    void unwatch(int fd, WatchFlag flag);
    void unwatchAll();

    static EventWatcher &getInstance();

private:
    using FdAndFlag = std::pair<int, WatchFlag>;
    using IWatchCallbackPtr = IWatchCallback*;

    int epoll_fd_;
    int early_wakeup_pipe_fd_[2]{};

    std::shared_mutex mutex_;
    std::map<FdAndFlag, IWatchCallbackPtr> callbacks_{};

    std::atomic<bool> running_{true};

    EpollWaitFunc epoll_impl_;
    std::unique_ptr<std::thread> loop_thread_;

    std::vector<int> readable_fd_{};
    std::vector<int> writable_fd_{};

    // Main loop for waiting on and processing events
    void waitLoop();

    void signalWakeLoop() const;
};
} // getrafty::rpc-io::io
