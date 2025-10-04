#pragma once

#include <chrono>

namespace getrafty::rpc::io {

class IClock {
public:
  virtual ~IClock() = default;
  [[nodiscard]] virtual std::chrono::time_point<std::chrono::milliseconds> time() const = 0;
};

class StopWatch {
public:
  explicit StopWatch(std::shared_ptr<IClock> clock) : clock_(std::move(clock)), before_(clock->time()) {};

  [[nodiscard]] std::chrono::milliseconds elapsed() const {
    const auto after = clock_->time();
    return std::chrono::duration_cast<std::chrono::milliseconds>(after - before_);
  }

private:
  std::shared_ptr<IClock> clock_;
  std::chrono::time_point<std::chrono::milliseconds> before_;
};





}