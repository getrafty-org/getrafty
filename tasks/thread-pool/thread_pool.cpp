#include <thread_pool.hpp>

#include <cassert>
#include <iostream>

namespace getrafty::wheels::concurrent {

ThreadPool::ThreadPool(const size_t threads)
    : state_(NONE), worker_threads_count_(threads) {}

void ThreadPool::start() {
  assert(state_.exchange(RUNNING) == NONE);

  // ==== YOUR CODE: @70e1 ====
  throw std::runtime_error(/*TODO:*/"start()");
  // ==== END YOUR CODE ====
}

ThreadPool::~ThreadPool() {
  if(state_.exchange(STOPPED) != STOPPED) {
    stop();
  }
}

bool ThreadPool::submit(Task&& task) {
  if (state_.load() != RUNNING) {
    return false;
  }

  worker_queue_.put(std::move(task));
  return true;
}

void ThreadPool::stop() {
  // ==== YOUR CODE: @606a ====
  throw std::runtime_error(/*TODO:*/"stop()");
  // ==== END YOUR CODE ====
}
}  // namespace getrafty::wheels::concurrent