#include <cassert>

#include "thread_pool.hpp"
#include "logging.hpp"

namespace getrafty::wheels::concurrent {
    ThreadPool::ThreadPool(const size_t threads)
        : worker_threads_count_(threads), wg_(), state_(NONE) {
    }

    void ThreadPool::start() {
        assert(state_.exchange(RUNNING) == NONE);

        for (uint32_t i = 0; i < worker_threads_count_; ++i) {
            worker_threads_.emplace_back([i, this] {
                std::optional<std::exception> ex;
                while (true) {
                    auto item = worker_queue_.take();
                    if (!item) {
                        // stop requested
                        break;
                    }
                    try {
                        (*item)();
                        wg_.done();
                    } catch (const std::exception &e) {
                        ex=e;
                        break;
                    }
                }

                if (ex) {
                    LOG(ERROR) <<"Worker thread " << i << " has been stopped abnormally due to unhandled exception: " << ex->what();
                } else {
                    LOG(TRACE) << "Worker thread " << i << " has been stopped";
                }
            });
        }
    }

    ThreadPool::~ThreadPool() {
        if (state_.load() != STOPPED) {
            LOG(WARNING) << "ThreadPool has not been properly stopped before destroy";
        }
    }

    bool ThreadPool::submit(Task&& task) {
        if (state_.load() != RUNNING) {
            return false;
        }

        wg_.add(1);
        worker_queue_.put({task});

        return true;
    }

    void ThreadPool::waitIdle() {
        wg_.wait();
    }

    void ThreadPool::stop() {
        assert(state_.exchange(STOPPING) == RUNNING);

        LOG(TRACE) << "Thread pool shutdown requested";

        for (uint32_t i = 0; i < worker_threads_count_; ++i) {
            worker_queue_.put(std::nullopt);
        }

        LOG(TRACE) << "Waiting for worker threads to join";
        for (auto &th: worker_threads_) {
            th.join();
        }

        state_.store(STOPPED);
        LOG(TRACE) << "Thread pool stopped";
    }
} // namespace getrafty::wheels::concurrent
