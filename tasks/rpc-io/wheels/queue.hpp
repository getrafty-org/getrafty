#pragma once

#include <atomic>
#include <cassert>
#include <functional>
#include <vector>
#include <deque>

namespace getrafty::wheels::concurrent {
    // Unbounded blocking multi-producers/multi-consumers (MPMC) queue
    template<typename T>
    class UnboundedBlockingQueue {
    public:
        UnboundedBlockingQueue() = default;

        // Non-copyable
        UnboundedBlockingQueue(const UnboundedBlockingQueue &) = delete;

        UnboundedBlockingQueue &operator=(const UnboundedBlockingQueue &) = delete;

        // Non-movable
        UnboundedBlockingQueue(UnboundedBlockingQueue &&) = delete;

        ~UnboundedBlockingQueue() = default;

        void put(T v) {
            std::unique_lock lock(mutex_);

            if (q_.empty()) {
                cv_is_empty_.notify_all();
            }

            q_.emplace_back(std::move(v));
        }

        T take() {
            std::unique_lock lock(mutex_);

            cv_is_empty_.wait(
                lock, [this] { return !(q_.empty()); });

            // if (q_.empty()) {
            //     return std::optional<T>{};
            // }

            auto v = std::move(q_.front());
            q_.pop_front();

            return std::move(v);
        }

    private:
        std::deque<T> q_;
        std::mutex mutex_;
        std::condition_variable cv_is_empty_;
    };
} // getrafty::wheels::concurrent
