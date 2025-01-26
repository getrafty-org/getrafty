#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <cerrno>

#include <fmt/format.h>
#include <sys/epoll.h>
#include <event_watcher.hpp>
#include <exception>
#include <iostream>

constexpr int kMaxEvents = 128;

namespace getrafty::rpc::io {

EventWatcher& EventWatcher::getInstance() {
  static EventWatcher ew;
  return ew;
}

EventWatcher::EventWatcher(EpollWaitFunc epoll_impl)
    : epoll_fd_(epoll_create1(0)), epoll_impl_(std::move(epoll_impl)) {

  assert(epoll_fd_ != -1);
  assert(pipe(early_wakeup_pipe_fd_) == 0);
  fcntl(early_wakeup_pipe_fd_[0], F_SETFL, O_NONBLOCK);

  epoll_event event{};
  event.events = EPOLLIN;
  event.data.fd = early_wakeup_pipe_fd_[0];
  epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, early_wakeup_pipe_fd_[0], &event);

  loop_thread_ = std::make_unique<std::thread>(&EventWatcher::waitLoop, this);
}

EventWatcher::~EventWatcher() {
  unwatchAll();
  running_.store(false, std::memory_order_release);
  loop_thread_->join();
  for (const auto fd :
       {epoll_fd_, early_wakeup_pipe_fd_[0], early_wakeup_pipe_fd_[1]}) {
    close(fd);
  }
}

void EventWatcher::signalWakeLoop() const {
  // Wake up the epoll wait loop if it's blocked
  constexpr char tmp = 1;
  assert(write(early_wakeup_pipe_fd_[1], &tmp, sizeof(tmp)) == 1);
}

void EventWatcher::watch(const int fd, const WatchFlag flag,
                         IWatchCallback* ch) {
  std::unique_lock lock(mutex_);

  if (!running_.load(std::memory_order_acquire)) {
    throw std::runtime_error("not running");
  }

  epoll_event event{};
  event.data.fd = fd;
  if (flag == CB_RDONLY) {
    event.events = EPOLLIN;
  } else if (flag == CB_WRONLY) {
    event.events = EPOLLOUT;
  } else {
    throw std::runtime_error("unknown flag");
  }

  epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event);

  callbacks_[{fd, flag}] = ch;

  signalWakeLoop();
}

void EventWatcher::unwatch(const int fd, const WatchFlag flag) {
  std::unique_lock lock(mutex_);

  epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
  callbacks_.erase({fd, flag});

  signalWakeLoop();
}

void EventWatcher::unwatchAll() {
  std::unique_lock lock(mutex_);
  for (const auto& [tag, _] : callbacks_) {
    // Don't remove the wakeup pipe from epoll
    if (tag.first != early_wakeup_pipe_fd_[0]) {
      epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, tag.first, nullptr);
    }
  }
  callbacks_.clear();

  signalWakeLoop();
}

void EventWatcher::waitLoop() {
  while (running_.load(std::memory_order_acquire)) {
    epoll_event events[kMaxEvents];

    const int n_fd = epoll_impl_(epoll_fd_, events, kMaxEvents, /*timeout=*/-1);  // NOLINT

    // ==== YOUR CODE: @59acb300 ====
    if (n_fd == -1) {
      if (errno == EINTR) {
        continue;
      }

      std::cerr << "EventWatcher::waitLoop failed with unrecoverable error, errno: "
                << strerror(errno);
      running_.store(false, std::memory_order_release);
      return;
    }

    readable_fd_.clear();
    readable_fd_.clear();
    for (int n = 0; n < n_fd; ++n) {
      if (int fd = events[n].data.fd; fd == early_wakeup_pipe_fd_[0]) {
        // Drain the pipe fully to avoid repeated triggers
        while (true) {
          char buffer[128];
          if (const ssize_t bytes = read(fd, buffer, sizeof(buffer)); bytes <= 0) {
            break;
          }
        }
      } else {
        if (events[n].events & EPOLLIN) {
          readable_fd_.push_back(fd);
        }
        if (events[n].events & EPOLLOUT) {
          writable_fd_.push_back(fd);
        }
      }
    }

    for (int fd : readable_fd_) {
      IWatchCallbackPtr cob = nullptr;
      {
        std::shared_lock lock(mutex_);
        if (auto it = callbacks_.find({fd, CB_RDONLY}); it != callbacks_.end()) {
          cob = it->second;
        }
      }

      try {
        if (cob) {
          cob->onReadReady(fd);
        }
      } catch ([[maybe_unused]] std::exception& ex) {
        // TODO: handle
      }
    }

    for (int fd : writable_fd_) {
      IWatchCallbackPtr cob = nullptr;
      {
        std::shared_lock lock(mutex_);
        if (auto it = callbacks_.find({fd, CB_WRONLY}); it != callbacks_.end()) {
          cob = it->second;
        }
      }

      try {
        if (cob) {
          cob->onWriteReady(fd);
        }
      } catch ([[maybe_unused]] std::exception& ex) {
        // TODO: handle
      }
    }
    // ==== END YOUR CODE ====
  }
}
}  // namespace getrafty::rpc::io
