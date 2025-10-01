#pragma once

#include <mutex>
#include <condition_variable>

namespace getrafty::wheels::concurrent {
class WaitGroup {
 public:
  void add(const size_t tickets) {
    std::unique_lock lock(mutex_);
    pending_tickets_ += tickets;
  }

  void done() {
    std::unique_lock lock(mutex_);
    --pending_tickets_;
    if (pending_tickets_ == 0) {
      cv_has_pending_tickets_.notify_all();
    }
  }

  void wait() {
    std::unique_lock lock(mutex_);
    cv_has_pending_tickets_.wait(lock,
                                 [this] { return pending_tickets_ == 0; });
  }

 private:
  std::mutex mutex_;
  size_t pending_tickets_{0};
  std::condition_variable cv_has_pending_tickets_;
};
}  // namespace getrafty::wheels::concurrent
