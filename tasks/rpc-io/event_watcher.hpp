#pragma once

#include <unordered_set>
#include <sys/select.h>
#include <vector>
#include <sys/epoll.h>
#include <thread_pool.hpp>

using getrafty::wheels::concurrent::ThreadPool;

namespace getrafty::rpc::io {
    enum WatchFlag {
        CB_NONE = 0x0,
        CB_RDONLY = 0x1,
        CB_WRONLY = 0x10,
        CB_RDWR = 0x11,
        CB_MASK = ~0x11,
    };

    class IWatchCallback {
    public:
        virtual void onReadReady(int fd) = 0;

        virtual void onWriteReady(int fd) = 0;

        virtual ~IWatchCallback() = default;
    };

    class EventWatcher {
    public:
        explicit EventWatcher(std::unique_ptr<ThreadPool> tp);

        explicit EventWatcher(
            std::unique_ptr<ThreadPool> tp,
            std::function<int(int, epoll_event*, int, int)> epollWaitFunc);

        ~EventWatcher();

        void watch(int fd, WatchFlag flag, IWatchCallback *ch);

        void unwatch(int fd);

        void unwatchAll();

        static EventWatcher &getInstance();

    private:
        // Main loop for waiting on and processing events
        void waitLoop();

        int epoll_fd_;
        int pipe_fd_[2]{}; // Pipe for wake-up notifications
        std::unique_ptr<ThreadPool> tp_;

        std::unordered_map<int, IWatchCallback *> callbacks_; // Callback map for each fd
        std::mutex m_;

        std::unordered_map<int, bool> scheduled_;

        std::function<int(int, epoll_event*, int, int)> epollWaitFunc_;
    };
} // getrafty::rpc-io::io
