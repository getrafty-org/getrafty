#pragma once

#include <deque>
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
    // ==== YOUR CODE: @b27057d1 ====
    std::unique_lock lock(mutex_);
    q_.emplace_back(std::move(v));
    // ==== END YOUR CODE ====
  }

  T take() {
    // ==== YOUR CODE: @48ddb17c ====
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
    // ==== END YOUR CODE ====
  }

 private:
  std::deque<T> q_;
  std::mutex mutex_;
};
}  // namespace getrafty::wheels::concurrent
