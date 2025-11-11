#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <unordered_map>
#include <utility>

#include <sys/epoll.h>

#include <bits/queue.hpp>
#include "hash.hpp"

namespace getrafty::io {

using WatchCallback    = std::move_only_function<void()>;
using WatchCallbackPtr = std::shared_ptr<WatchCallback>;

using EpollWaitFunc = std::move_only_function<int(int, epoll_event*, int, int)>;

enum WatchFlag : uint8_t {
  RDONLY = 0x00,
  WRONLY = 0x01,
};

namespace detail {
struct Pipe {
  Pipe();
  ~Pipe();
  int read_end_;
  int write_end_;
};
}  // namespace detail

class EventWatcher {
 public:
  explicit EventWatcher(EpollWaitFunc epoll_impl = ::epoll_wait);
  virtual ~EventWatcher();

  EventWatcher(const EventWatcher&)            = delete;
  EventWatcher& operator=(const EventWatcher&) = delete;
  EventWatcher(EventWatcher&&)                 = delete;
  EventWatcher& operator=(EventWatcher&&)      = delete;

  virtual void watch(int fd, WatchFlag flag, WatchCallback callback);
  virtual void unwatch(int fd, WatchFlag flag);
  virtual void unwatchAll();
  virtual void runInEventWatcherLoop(WatchCallback task);

 private:
  using W = std::pair<int, WatchFlag>;

  int epoll_fd_;
  detail::Pipe wakeup_pipe_;

  std::unordered_map<W, WatchCallbackPtr, bits::Hash<W>> callbacks_;

  std::atomic<bool> running_{false};
  bits::MPSCQueue<WatchCallback> task_queue_;

  EpollWaitFunc epoll_impl_;
  std::unique_ptr<std::thread> loop_thread_;

  void waitLoop();
  void wakeup() const;
  void onWakeup(int fd);
  void invokeCallback(int fd, WatchFlag flag);
};

}  // namespace getrafty::io
