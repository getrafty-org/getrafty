#include "event_watcher.hpp"

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <exception>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <fmt/format.h>

namespace getrafty::rpc::io {

namespace detail {

[[maybe_unused]] constexpr int kMaxEvents = 128;

int createEpollFd() {
  const int fd = ::epoll_create1(EPOLL_CLOEXEC);
  if (fd == -1) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to create epoll fd");
  }
  return fd;
}

std::pair<int, int> createPipe() {
  std::array<int, 2> fds{-1, -1};
  if (::pipe(fds.data()) != 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to create pipe");
  }

  const int flags = ::fcntl(fds[0], F_GETFL, 0);
  if (flags == -1) {
    ::close(fds[0]);
    ::close(fds[1]);
    throw std::system_error(errno, std::generic_category(),
                            "Failed to get pipe flags");
  }

  if (::fcntl(fds[0], F_SETFL, flags | O_NONBLOCK) == -1) {
    ::close(fds[0]);
    ::close(fds[1]);
    throw std::system_error(errno, std::generic_category(),
                            "Failed to set pipe non-blocking");
  }

  return {fds[0], fds[1]};
}

FileDescriptor::~FileDescriptor() {
  if (fd_ != -1) {
    ::close(fd_);
  }
}

FileDescriptor::FileDescriptor(FileDescriptor&& other) noexcept
    : fd_(other.fd_) {
  other.fd_ = -1;
}

FileDescriptor& FileDescriptor::operator=(FileDescriptor&& other) noexcept {
  if (this != &other) {
    if (fd_ != -1) {
      ::close(fd_);
    }
    fd_ = other.fd_;
    other.fd_ = -1;
  }
  return *this;
}

int FileDescriptor::release() noexcept {
  const int fd = fd_;
  fd_ = -1;
  return fd;
}

Pipe::Pipe() {
  auto [read_fd, write_fd] = createPipe();
  read_fd_ = FileDescriptor(read_fd);
  write_fd_ = FileDescriptor(write_fd);
}

}  // namespace detail

EventWatcher::EventWatcher(EpollWaitFunc epoll_impl)
    : epoll_fd_(detail::createEpollFd()),
      wakeup_pipe_(detail::Pipe{}),
      epoll_impl_{std::move(epoll_impl)} {
  epoll_event event{};
  event.events = EPOLLIN;
  event.data.fd = wakeup_pipe_.read_fd();

  if (::epoll_ctl(epoll_fd_.get(), EPOLL_CTL_ADD, wakeup_pipe_.read_fd(),
                  &event) == -1) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to add wakeup pipe to epoll");
  }

  running_.store(true, std::memory_order_relaxed);
  loop_thread_ = std::make_unique<std::thread>(&EventWatcher::waitLoop, this);
}

EventWatcher::~EventWatcher() {
  if (running_.exchange(false, std::memory_order_relaxed)) {
    unwatchAll();
    wakeup();

    if (loop_thread_ && loop_thread_->joinable()) {
      loop_thread_->join();
    }
  }
}

void EventWatcher::wakeup() const {
  constexpr char signal = 1;
  ::write(wakeup_pipe_.write_fd(), &signal, sizeof(signal));
}

void EventWatcher::onWakeup(const int fd) const {
  std::array<char, 1024> buffer{};
  while (true) {
    const auto bytes = ::read(fd, buffer.data(), buffer.size());
    if (bytes <= 0) {
      break;
    }
  }
}

void EventWatcher::watch(const int fd, const WatchFlag flag,
                         IWatchCallbackPtr callback) {

  if (!running_.load(std::memory_order_relaxed)) {
    throw std::runtime_error("not running");
  }

  std::cerr << "[EventWatcher::watch] fd=" << fd << " flag=" << (int)flag << "\n";

  epoll_event event{};
  event.data.fd = fd;
  event.events = 0;

  bool fd_in_epoll = false;
  {
    const std::unique_lock lock{mutex_};

    auto [it, inserted] = callbacks_.insert_or_assign({fd, flag}, std::move(callback));

    if (!inserted) {
      std::cerr << "[EventWatcher::watch] Callback already exists - calling wakeup and returning\n";
      wakeup();
      return;
    }

    std::cerr << "[EventWatcher::watch] New callback registered\n";

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
  }

  const int op = fd_in_epoll ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
  if (::epoll_ctl(epoll_fd_.get(), op, fd, &event) == -1) {
    throw std::system_error(errno, std::generic_category(),
                            fmt::format("failed to {} fd {} to epoll",
                                        fd_in_epoll ? "modify" : "add", fd));
  }

  wakeup();
}

void EventWatcher::unwatch(const int fd, const WatchFlag flag) {
  if (!running_.load(std::memory_order_relaxed)) {
    return;
  }

  epoll_event event{};
  event.data.fd = fd;
  event.events = 0;

  bool exists = false;
  {
    std::unique_lock lock(mutex_);
    callbacks_.erase({fd, flag});

    if (flag == RDONLY) {
      if (callbacks_.contains({fd, WRONLY})) {
        exists = true;
        event.events |= EPOLLOUT;
      }
    } else if (flag == WRONLY) {
      if (callbacks_.contains({fd, RDONLY})) {
        exists = true;
        event.events |= EPOLLIN;
      }
    }
  }

  const int op = exists ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
  if (::epoll_ctl(epoll_fd_.get(), op, fd, &event) == -1 && errno != ENOENT) {
    throw std::system_error(errno, std::generic_category(),
                            fmt::format("failed to {} fd {} in epoll",
                                        exists ? "modify" : "remove", fd));
  }

  wakeup();
}

void EventWatcher::unwatchAll() {
  if (!running_.load(std::memory_order_relaxed)) {
    return;
  }

  {
    const std::unique_lock lock{mutex_};
    for (auto it = callbacks_.begin(); it != callbacks_.end();) {
      const auto [fd, flag] = it->first;
      if (fd != wakeup_pipe_.read_fd() && fd != wakeup_pipe_.write_fd()) {
        ::epoll_ctl(epoll_fd_.get(), EPOLL_CTL_DEL, fd, nullptr);
        it = callbacks_.erase(it);
      } else {
        ++it;
      }
    }
  }

  wakeup();
}

void EventWatcher::invokeCallback(const int fd, const WatchFlag flag) {
  // ==== YOUR CODE: @67d9 ====
  throw std::runtime_error(fmt::format(/*TODO:*/ "invokeCallback({},{})", fd,
                                       static_cast<int>(flag)));
  // ==== END YOUR CODE ====
}

void EventWatcher::waitLoop() {
  // ==== YOUR CODE: @1879 ====
  throw std::runtime_error(/*TODO:*/ "waitLoop()");
  // ==== END YOUR CODE ====
}

}  // namespace getrafty::rpc::io
