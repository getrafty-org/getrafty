#pragma once

#include <deque>
#include <mutex>
#include <folly/experimental/coro/Baton.h>
#include <folly/experimental/coro/Task.h>

namespace getrafty::wheels::concurrent {

template <typename T>
class UnboundedBlockingQueue {
public:
  UnboundedBlockingQueue() = default;
  UnboundedBlockingQueue(const UnboundedBlockingQueue&) = delete;
  UnboundedBlockingQueue& operator=(const UnboundedBlockingQueue&) = delete;
  UnboundedBlockingQueue(UnboundedBlockingQueue&&) = delete;
  ~UnboundedBlockingQueue() = default;

  // Can be called from either coroutine or non-coroutine context.
  void put(T v) {
    std::unique_lock lock(mutex_);
    if (!waiters_.empty()) {
      // If a coroutine is waiting, resume it with the new value.
      Waiter* waiter = waiters_.front();
      waiters_.pop_front();
      waiter->value = std::move(v);
      waiter->baton.post();
    } else {
      q_.emplace_back(std::move(v));
    }
  }

  // Must be called from a coroutine context.
  // This function is co_await‑able and will suspend until a value is available.
  folly::coro::Task<T> take() {
    Waiter waiter;
    {
      std::unique_lock lock(mutex_);
      if (!q_.empty()) {
        T value = std::move(q_.front());
        q_.pop_front();
        co_return std::move(value);
      }
      // No available item, so add this waiter.
      waiters_.push_back(&waiter);
    }
    co_await waiter.baton; // suspend until put() posts the baton
    co_return std::move(waiter.value);
  }

private:
  // A waiter for a suspended take.
  struct Waiter {
    folly::coro::Baton baton;
    T value; // will be assigned when put() is called
  };

  std::deque<T> q_;
  std::mutex mutex_;
  std::deque<Waiter*> waiters_;
};

}  // namespace getrafty::wheels::concurrent
