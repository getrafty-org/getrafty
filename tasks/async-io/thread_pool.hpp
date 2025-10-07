#pragma once

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>
#include <optional>
#include <functional>

#include "queue.hpp"

namespace getrafty::concurrent {

using Task = std::move_only_function<void()>;

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
  [[maybe_unused]] uint32_t worker_threads_count_;
  BlockingMPMCQueue<std::optional<Task>> worker_queue_;
  std::vector<std::thread> worker_threads_;
};

}  // namespace getrafty::concurrent
