#include <cassert>
#include <iostream>
#include <optional>

#include <thread_pool.hpp>

namespace getrafty::wheels::concurrent {
ThreadPool::ThreadPool(const size_t threads)
    : state_(NONE), worker_threads_count_(threads) {}

void ThreadPool::start() {
  assert(state_.exchange(RUNNING) == NONE);

  // ==== YOUR CODE: @70e177c1 ====
  for (uint32_t i = 0; i < worker_threads_count_; ++i) {
    worker_threads_.emplace_back([this] {
      while (true) {
        auto item = worker_queue_.take();
        if (!item) {
          // stop
          break;
        }
        try {
          (*item)();
        } catch (std::exception& ex) {
          std::cerr << "unhandled exception in ThreadPool thread: " << ex.what()
                    << std::endl;
        }
      }
    });
  }
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

  worker_queue_.put({task});
  return true;
}

void ThreadPool::stop() {
  // ==== YOUR CODE: @606a035f ====
  if (state_.exchange(STOPPING) == RUNNING) {
    for (uint32_t i = 0; i < worker_threads_count_; ++i) {
      worker_queue_.put(std::nullopt);
    }
    for (auto& th : worker_threads_) {
      th.join();
    }

    state_.store(STOPPED);
  }
  // ==== END YOUR CODE ====
}
}  // namespace getrafty::wheels::concurrent