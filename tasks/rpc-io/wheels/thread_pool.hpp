#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>
#include "queue.hpp"

#include "wait_group.hpp"

namespace getrafty::wheels::concurrent {

    using Task = std::function<void()>;

    // Fixed-size pool of worker threads

    class ThreadPool {
    public:
        explicit ThreadPool(size_t threads);

        ~ThreadPool();

        // Non-copyable
        ThreadPool(const ThreadPool &) = delete;

        ThreadPool &operator=(const ThreadPool &) = delete;

        // Non-movable
        ThreadPool(ThreadPool &&) = delete;

        ThreadPool &operator=(ThreadPool &&) = delete;

        void start();

        bool submit(Task&&);

        void waitIdle();

        void stop();

    private:
        enum State : uint8_t {
            NONE,
            RUNNING,
            STOPPING,
            STOPPED
        };

        uint32_t worker_threads_count_;
        WaitGroup wg_;
        UnboundedBlockingQueue<std::optional<Task>> worker_queue_{};
        std::atomic<State> state_;
        std::vector<std::thread> worker_threads_;
    };
} // namespace getrafty::wheels::concurrent
