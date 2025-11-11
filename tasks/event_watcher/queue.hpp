#pragma once

// ==== YOUR CODE: @138b ====
#include <optional>
// ==== END YOUR CODE ====

namespace getrafty::concurrent {
template <typename T>
class Queue {
 public:
  explicit Queue() = default;

  // Non-copyable
  Queue(const Queue&) = delete;

  Queue& operator=(const Queue&) = delete;

  // Non-movable
  Queue(Queue&&) = delete;

  ~Queue() = default;

  void push(T value) {
    // ==== YOUR CODE: @b270 ====

    // ==== END YOUR CODE ====
  }

  std::optional<T> tryTake() {
    // ==== YOUR CODE: @48dd ====

    // ==== END YOUR CODE ====
  }

 private:
  // ==== YOUR CODE: @be49 ====

  // ==== END YOUR CODE ====
};
}  // namespace getrafty::concurrent