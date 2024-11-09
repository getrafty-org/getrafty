#include <cassert>

#include "thread_pool.hpp"

namespace getrafty::wheels::concurrent {
ThreadPool::ThreadPool(const size_t threads)
    : worker_threads_count_(threads), state_(NONE) {}

void ThreadPool::start() {
  assert(state_.exchange(RUNNING) == NONE);
  // Your code goes here, initializing threads, etc.
}

ThreadPool::~ThreadPool() {
  assert(state_.load() != STOPPED);
}

bool ThreadPool::submit(Task&& task) {
  if (state_.load() != RUNNING) {
    return false;
  }

  worker_queue_.put({task});
  return true;
}

void ThreadPool::stop() {
  assert(state_.exchange(STOPPING) == RUNNING);

  for (uint32_t i = 0; i < worker_threads_count_; ++i) {
    worker_queue_.put(std::nullopt);
  }
  for (auto& th : worker_threads_) {
    th.join();
  }

  state_.store(STOPPED);
}
}  // namespace getrafty::wheels::concurrent