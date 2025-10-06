#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <shared_mutex>
#include <thread>
#include <utility>

#include <sys/epoll.h>

#include "thread_pool.hpp"

namespace getrafty::rpc::io {

using wheels::concurrent::ThreadPool;
using EpollWaitFunc = std::function<int(int, epoll_event*, int, int)>;

enum WatchFlag : uint8_t {
  RDONLY = 0x00,
  WRONLY = 0x01,
};

class IWatchCallback {
 public:
  virtual void run(int /*fd*/) {};
  virtual ~IWatchCallback() = default;
};

using IWatchCallbackPtr = std::shared_ptr<IWatchCallback>;

namespace detail {

class FileDescriptor {
 public:
  FileDescriptor() = default;
  explicit FileDescriptor(int fd) : fd_(fd) {}
  ~FileDescriptor();

  FileDescriptor(const FileDescriptor&) = delete;
  FileDescriptor& operator=(const FileDescriptor&) = delete;
  FileDescriptor(FileDescriptor&& other) noexcept;
  FileDescriptor& operator=(FileDescriptor&& other) noexcept;

  [[nodiscard]] int get() const noexcept { return fd_; }
  [[nodiscard]] bool valid() const noexcept { return fd_ != -1; }
  int release() noexcept;

 private:
  int fd_{-1};
};

class Pipe {
 public:
  Pipe();
  ~Pipe() = default;

  Pipe(const Pipe&) = delete;
  Pipe& operator=(const Pipe&) = delete;
  Pipe(Pipe&&) = default;
  Pipe& operator=(Pipe&&) = default;

  [[nodiscard]] int read_fd() const noexcept { return read_fd_.get(); }
  [[nodiscard]] int write_fd() const noexcept { return write_fd_.get(); }

 private:
  FileDescriptor read_fd_;
  FileDescriptor write_fd_;
};

}  // namespace detail

class EventWatcher {
 public:
  explicit EventWatcher(EpollWaitFunc epoll_impl = ::epoll_wait);
  virtual ~EventWatcher();

  EventWatcher(const EventWatcher&) = delete;
  EventWatcher& operator=(const EventWatcher&) = delete;
  EventWatcher(EventWatcher&&) = delete;
  EventWatcher& operator=(EventWatcher&&) = delete;

  virtual void watch(int fd, WatchFlag flag, IWatchCallbackPtr callback);
  virtual void unwatch(int fd, WatchFlag flag);
  void unwatchAll();

 private:
  using FdAndFlag = std::pair<int, WatchFlag>;

  detail::FileDescriptor epoll_fd_;
  detail::Pipe wakeup_pipe_;

  std::shared_mutex mutex_;
  std::map<FdAndFlag, IWatchCallbackPtr> callbacks_;

  std::atomic<bool> running_{false};

  EpollWaitFunc epoll_impl_;
  std::unique_ptr<std::thread> loop_thread_;

  void waitLoop();
  void wakeup() const;
  void onWakeup(int fd) const;
  void invokeCallback(int fd, WatchFlag flag);
};

}  // namespace getrafty::rpc::io
