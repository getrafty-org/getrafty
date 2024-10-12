#pragma once

#include <cstdlib>
#include <mutex>

namespace getrafty::wheels::concurrent {

class WaitGroup {
   public:
    // += count
    void add(size_t tickets) {
        std::unique_lock lock(mutex_);
        pending_tickets_ += tickets;
    }

    // =- 1
    void done() {
        std::unique_lock lock(mutex_);
        --pending_tickets_;
        if (pending_tickets_ == 0) {
            cv_has_pending_tickets_.notify_all();
        }
    }

    // == 0
    // One-shot
    void wait() {
        std::unique_lock lock(mutex_);
        cv_has_pending_tickets_.wait(lock,
                                     [this] { return pending_tickets_ == 0; });
    }

   private:
    size_t pending_tickets_{0};
    std::mutex mutex_;
    std::condition_variable cv_has_pending_tickets_;
};

}  // namespace getrafty::wheels::concurrent