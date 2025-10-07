#pragma once

#include <fmt/format.h>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace getrafty::concurrent {
// Unbounded blocking multi-producers/multi-consumers queue
template <typename T>
class BlockingMPMCQueue {
 public:
  BlockingMPMCQueue() = default;

  // Non-copyable
  BlockingMPMCQueue(const BlockingMPMCQueue&) = delete;

  BlockingMPMCQueue& operator=(const BlockingMPMCQueue&) = delete;

  // Non-movable
  BlockingMPMCQueue(BlockingMPMCQueue&&) = delete;

  ~BlockingMPMCQueue() = default;

  void put(T v) {
    // ==== YOUR CODE: @b270 ====
    {
      std::unique_lock lock(mutex_);
      q_.emplace_back(std::move(v));
    }
    // ==== END YOUR CODE ====
  }

  T take() {
    // ==== YOUR CODE: @48dd ====
    while (true) {
      std::unique_lock lock(mutex_);
      if (!q_.empty()) {
        auto v = std::move(q_.front());
        q_.pop_front();
        return v;
      }
    }

    // ==== END YOUR CODE ====
  }

 private:
  std::deque<T> q_;
  std::mutex mutex_;
  [[maybe_unused]] std::condition_variable cv_;
};
}  // namespace getrafty::concurrent
