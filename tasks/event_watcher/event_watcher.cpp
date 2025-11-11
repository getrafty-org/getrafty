#include "event_watcher.hpp"

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <bits/ttl/logger.hpp>
#include <bits/util.hpp>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <system_error>
#include <utility>

namespace getrafty::io {

namespace detail {
Pipe::Pipe() {
  auto [read_end, write_end] = bits::makePipe();
  read_end_                  = read_end;
  write_end_                 = write_end;
}

Pipe::~Pipe() {
  close(write_end_);
  close(read_end_);
}
}  // namespace detail

EventWatcher::EventWatcher(EpollWaitFunc epoll_impl)
    : epoll_fd_(bits::makeEpoll()), epoll_impl_{std::move(epoll_impl)} {
  epoll_event event{};
  event.events  = EPOLLIN;
  event.data.fd = wakeup_pipe_.read_end_;
  
  if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wakeup_pipe_.read_end_, &event) ==
      -1) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to add wakeup pipe to epoll");
  }

  running_.store(true, std::memory_order_release);
  loop_thread_ = std::make_unique<std::thread>(&EventWatcher::waitLoop, this);
}

EventWatcher::~EventWatcher() {
  if (running_.exchange(false, std::memory_order_acq_rel)) {
    unwatchAll();
    if (loop_thread_ && loop_thread_->joinable()) {
      loop_thread_->join();
    }
    ::close(epoll_fd_);
  }
}

void EventWatcher::wakeup() const {
  constexpr char signal = 1;
  while (true) {
    const auto written =
        ::write(wakeup_pipe_.write_end_, &signal, sizeof(signal));
    if (written == sizeof(signal)) {
      break;
    }

    if (written == -1 && errno == EINTR) {
      continue;
    }

    if (written == -1 && errno == EAGAIN) {
      break;
    }

    TTL_LOG(bits::ttl::Critical) << "Wakeup failed: errno=" << errno;
    return;
  }
}

void EventWatcher::onWakeup(const int fd) {
  std::array<char, sizeof(char)> buffer{};
  ::read(fd, buffer.data(), buffer.size());

  while (auto task = task_queue_.tryTake()) {
    if (!task) {
      continue;
    }

    if (auto fn = std::move(*task)) {
      try {
        fn();
      } catch (const std::exception& ex) {
        TTL_LOG(bits::ttl::Error) << "Exception in task: " << ex.what();
      } catch (...) {
        TTL_LOG(bits::ttl::Error) << "Unknown exception in task";
      }
    }
  }
}

void EventWatcher::watch(const int fd, const WatchFlag flag,
                         WatchCallback callback) {
  if (running_.load(std::memory_order_acquire)) {
    runInEventWatcherLoop(
        [this, fd, flag,
         cb = std::make_shared<WatchCallback>(std::move(callback))] mutable {
          auto [_, inserted] = callbacks_.insert_or_assign({fd, flag}, cb);
          if (!inserted) {
            return;
          }

          epoll_event event{};
          event.data.fd = fd;
          event.events  = 0;

          bool fd_in_epoll = false;
          if (flag == RDONLY) {
            event.events |= EPOLLIN;
            if (callbacks_.contains({fd, WRONLY})) {
              fd_in_epoll = true;
              event.events |= EPOLLOUT;
            }
          } else if (flag == WRONLY) {
            event.events |= EPOLLOUT;
            if (callbacks_.contains({fd, RDONLY})) {
              fd_in_epoll = true;
              event.events |= EPOLLIN;
            }
          }

          const int op = fd_in_epoll ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
          if (::epoll_ctl(epoll_fd_, op, fd, &event) == -1) {
            TTL_LOG(bits::ttl::Critical) << "Watch failed: errno" << errno;
          }
        });
  }
}

void EventWatcher::unwatch(const int fd, const WatchFlag flag) {
  if (running_.load(std::memory_order_acquire)) {
    runInEventWatcherLoop([this, fd, flag] {
      epoll_event event{};
      event.data.fd = fd;
      event.events  = 0;

      bool fd_in_epoll = false;

      callbacks_.erase({fd, flag});

      if (flag == RDONLY) {
        if (callbacks_.contains({fd, WRONLY})) {
          fd_in_epoll = true;
          event.events |= EPOLLOUT;
        }
      } else if (flag == WRONLY) {
        if (callbacks_.contains({fd, RDONLY})) {
          fd_in_epoll = true;
          event.events |= EPOLLIN;
        }
      }

      const int op = fd_in_epoll ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
      if (::epoll_ctl(epoll_fd_, op, fd, &event) == -1) {
        TTL_LOG(bits::ttl::Critical) << "Unwatch failed: errno" << errno;
      }
    });
  }
}

void EventWatcher::unwatchAll() {
  if (running_.load(std::memory_order_acquire)) {
    runInEventWatcherLoop([this] {
      for (auto it = callbacks_.begin(); it != callbacks_.end();) {
        const auto [fd, flag] = it->first;
        if (fd != wakeup_pipe_.read_end_ && fd != wakeup_pipe_.write_end_) {
          ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
          it = callbacks_.erase(it);
        } else {
          ++it;
        }
      }
    });
  }

  wakeup();
}

void EventWatcher::invokeCallback(const int fd, const WatchFlag flag) {
  // ==== YOUR CODE: @67d9 ====

  // ==== END YOUR CODE ====
}

void EventWatcher::waitLoop() {
  // ==== YOUR CODE: @1879 ====

  // ==== END YOUR CODE ====
}

void EventWatcher::runInEventWatcherLoop(WatchCallback task) {
  // ==== YOUR CODE: @6c13 ====

  // ==== END YOUR CODE ====
}

}  // namespace getrafty::io
