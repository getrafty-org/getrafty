#pragma once

#include <atomic>
#include <cstdint>
#include "folly/Function.h"
#include <thread>
#include <vector>
#include <optional>
#include "queue.hpp"

namespace getrafty::wheels::concurrent {

using Task = folly::Function<void()>;

// Fixed-size pool of worker threads
class ThreadPool {
 public:
  explicit ThreadPool(size_t threads);

  ~ThreadPool();

  // Non-copyable
  ThreadPool(const ThreadPool&) = delete;

  ThreadPool& operator=(const ThreadPool&) = delete;

  // Non-movable
  ThreadPool(ThreadPool&&) = delete;

  ThreadPool& operator=(ThreadPool&&) = delete;

  void start();

  bool submit(Task&&);

  void stop();

 private:
  enum State : uint8_t { NONE, RUNNING, STOPPING, STOPPED };

  std::atomic<State> state_;
  uint32_t worker_threads_count_;
  UnboundedBlockingQueue<std::optional<Task>> worker_queue_{};
  std::vector<std::thread> worker_threads_;
};

}  // namespace getrafty::wheels::concurrent
