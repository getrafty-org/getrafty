#include <event_watcher.hpp>

#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <cerrno>

#include <fmt/format.h>
#include <sys/epoll.h>

#include <cassert>
#include <exception>
#include <iostream>
#include <mutex>

namespace getrafty::rpc::io {
EventWatcher& EventWatcher::getInstance() {
  static EventWatcher ew{
      std::make_unique<ThreadPool>(std::thread::hardware_concurrency())};
  return ew;
}

EventWatcher::EventWatcher(std::unique_ptr<ThreadPool> tp)
    : EventWatcher(std::move(tp), ::epoll_wait) {};

EventWatcher::EventWatcher(
    std::unique_ptr<ThreadPool> tp,
    std::function<int(int, epoll_event*, int, int)> epollWaitFunc)
    : epoll_fd_(epoll_create1(0)),
      tp_(std::move(tp)),
      epollWaitFunc_(std::move(epollWaitFunc)) {
  assert(epoll_fd_ != -1);
  assert(pipe(pipe_fd_) == 0);
  fcntl(pipe_fd_[0], F_SETFL, O_NONBLOCK);

  // Add pipe read end to epoll for wake-up notifications
  epoll_event event{};
  event.events = EPOLLIN;
  event.data.fd = pipe_fd_[0];
  epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, pipe_fd_[0], &event);

  // Start the thread pool and event loop
  tp_->start();
  tp_->submit([&] { waitLoop(); });
}

EventWatcher::~EventWatcher() {
  unwatchAll();
  tp_->stop();
  for (const auto fd : {epoll_fd_, pipe_fd_[0], pipe_fd_[1]}) {
    close(fd);
  }
}

void EventWatcher::watch(const int fd, const WatchFlag flag,
                         IWatchCallback* ch) {
  std::lock_guard lock(m_);

  epoll_event event{};
  event.data.fd = fd;
  if (flag == CB_RDONLY) {
    event.events = EPOLLIN;
  } else if (flag == CB_WRONLY) {
    event.events = EPOLLOUT;
  } else {
    event.events = EPOLLIN | EPOLLOUT;
  }

  epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event);
  callbacks_[fd] = ch;

  // Wake up the epoll wait loop if it's blocked
  constexpr char tmp = 1;
  assert(write(pipe_fd_[1], &tmp, sizeof(tmp)) == 1);
}

void EventWatcher::unwatch(const int fd) {
  std::lock_guard lock(m_);

  epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
  callbacks_.erase(fd);

  // Notify the wait loop of changes
  constexpr char tmp = 1;
  assert(write(pipe_fd_[1], &tmp, sizeof(tmp)) == 1);
}

void EventWatcher::unwatchAll() {
  std::lock_guard lock(m_);
  for (const auto& [fd, _] : callbacks_) {
    if (fd != pipe_fd_[0] /*&& callbacks_[fd]*/) {
      epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    }
  }
  callbacks_.clear();

  // Notify the wait loop of changes
  constexpr char tmp = 1;
  assert(write(pipe_fd_[1], &tmp, sizeof(tmp)) == 1);
}

void EventWatcher::waitLoop() {
  constexpr int max_events = 64;
  epoll_event events[max_events];

  if (const int n_fd = epollWaitFunc_(epoll_fd_, events, max_events, -1);
      n_fd == -1) {
    if (errno == EINTR) {
      // Retry on EINTR
    } else {
      std::cerr << "Fatal error in epoll_wait: " << strerror(errno);
      std::abort();  // Fail fast on other errors
    }
  } else {
    // Your code goes here
  }

  // Your code goes here

  tp_->submit([&] { waitLoop(); });
}
}  // namespace getrafty::rpc::io
