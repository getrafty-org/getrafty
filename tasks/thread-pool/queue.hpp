#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>

namespace getrafty::concurrent {
// Unbounded blocking multi-producers/multi-consumers queue
template <typename T>
class Queue {
 public:
  Queue() = default;

  // Non-copyable
  Queue(const Queue&) = delete;

  Queue& operator=(const Queue&) = delete;

  // Non-movable
  Queue(Queue&&) = delete;

  ~Queue() = default;

  void put(T v) {
    // ==== YOUR CODE: @b270 ====

    // ==== END YOUR CODE ====
  }

  T take() {
    // ==== YOUR CODE: @48dd ====

    // ==== END YOUR CODE ====
  }

 private:
  std::deque<T> q_;
  std::mutex mutex_;
  [[maybe_unused]] std::condition_variable cv_;
};
}  // namespace getrafty::concurrent
