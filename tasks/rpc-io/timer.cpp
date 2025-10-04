#include "timer.hpp"

#include <sys/timerfd.h>
#include <unistd.h>
#include <cassert>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

using namespace std::chrono;

inline auto toITimerSpec(const milliseconds duration) -> itimerspec{
  itimerspec ts{};

  const auto secs = duration_cast<seconds>(duration);
  ts.it_value.tv_sec = secs.count();
  const auto nanos = duration_cast<nanoseconds>(duration - secs);
  ts.it_value.tv_nsec = nanos.count();
  return ts;
}

inline auto makeTimer(const milliseconds duration) -> /*FD:*/int {
  // https://man7.org/linux/man-pages/man2/timerfd_create.2.html
  const int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  assert(timer_fd != -1);

  const auto tp = toITimerSpec(duration);
  timerfd_settime(timer_fd, 0, &tp, nullptr);

  return timer_fd;
}


namespace getrafty::rpc::io {

Timer::~Timer() {
  tp_->stop();
}

TimerTicket Timer::schedule(const milliseconds duration, TimerCallback callback) {
  auto timer_fd = makeTimer(duration);
  const auto timer = std::make_shared<TimerState>(timer_fd, std::move(callback), *this);
  {
    std::lock_guard lock(timers_m_);
    timers_[timer_fd] = timer;
  }

  watcher_.watch(timer_fd, CB_RDONLY, &(*timer));

  return timer_fd;
};

bool Timer::cancel(const TimerTicket tt) {
  std::shared_ptr<TimerState> timer;
  {
    std::lock_guard lock(timers_m_);
    if (const auto it = timers_.find(tt); it != timers_.end()) {
      timer = std::move(it->second);
      timers_.erase(it);
    }
  }

  if (timer) {
    watcher_.unwatch(tt, CB_RDONLY);
    close(tt);
    return true;
  }

  return false;
}


void Timer::TimerState::onReadReady(const int fd) {
  assert(this->fd == fd);

  const auto self = shared_from_this();

  // ensure no further unnecessary epoll wake-ups
  uint64_t expirations;
  const ssize_t s = read(fd, &expirations, sizeof(expirations));
  assert(s == sizeof(expirations));

  {
    std::lock_guard lock(parent.timers_m_);
    parent.timers_.erase(fd);
  }

  parent.watcher_.unwatch(fd, CB_RDONLY);
  if(close(fd) == -1) {
    throw std::runtime_error("Failed to close timer fd");
  }

  if (self->callback) {
    parent.tp_->submit([self=self] { self->callback();});
  }
}

}