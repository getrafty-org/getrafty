#pragma once

#include <atomic>
#include <cassert>
#include <deque>
#include <functional>
#include <vector>
#include <mutex>

namespace getrafty::wheels::concurrent {
// Unbounded blocking multi-producers/multi-consumers queue
template <typename T>
class UnboundedBlockingQueue {
 public:
  UnboundedBlockingQueue() = default;

  // Non-copyable
  UnboundedBlockingQueue(const UnboundedBlockingQueue&) = delete;

  UnboundedBlockingQueue& operator=(const UnboundedBlockingQueue&) = delete;

  // Non-movable
  UnboundedBlockingQueue(UnboundedBlockingQueue&&) = delete;

  ~UnboundedBlockingQueue() = default;

  void put(T v) {
    std::unique_lock lock(mutex_);
    q_.emplace_back(std::move(v));
  }

  T take() {
    // Your code goes here instead of the below snippet
    while (true) {
      std::unique_lock lock(mutex_);
      if (q_.empty()) {
        // backoff?
        continue;
      }
      auto v = std::move(q_.front());
      q_.pop_front();

      return std::move(v);
    }
  }

 private:
  std::deque<T> q_;
  std::mutex mutex_;
};
}  // namespace getrafty::wheels::concurrent
