#pragma once

#include <memory>

#include "event_watcher.hpp"

namespace getrafty::rpc::io {
using TimerTicket = int;
using TimerCallback = std::function<void()>;

class Timer {
 public:
  explicit Timer(EventWatcher& watcher, std::shared_ptr<ThreadPool> tp)
    : watcher_(watcher), tp_(std::move(tp)){}

  ~Timer();

  TimerTicket schedule(std::chrono::milliseconds duration,
                       TimerCallback callback);

  bool cancel(TimerTicket tt);

 private:
  struct TimerState final : IWatchCallback,
                            std::enable_shared_from_this<TimerState> {
    explicit TimerState(const int fd, TimerCallback callback, Timer& parent)
        : fd(fd), callback(std::move(callback)), parent(parent) {}

    void onReadReady(int fd) override;

    int fd;
    TimerCallback callback;
    Timer& parent;
  };

  EventWatcher& watcher_;

  std::mutex timers_m_;
  std::unordered_map<int, std::shared_ptr<TimerState>> timers_;

  std::shared_ptr<ThreadPool> tp_;
};
}  // namespace getrafty::rpc::io
