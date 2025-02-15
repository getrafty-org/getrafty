#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include "event_watcher.hpp"
#include "timer.hpp"

#include <latch>

using namespace std::chrono;
using namespace std::chrono_literals;
using namespace getrafty::rpc::io;

using ::testing::StrictMock;

class TimerTest : public ::testing::Test {
 protected:
  EventWatcher& watcher = EventWatcher::getInstance();

  void TearDown() override { watcher.unwatchAll(); }
};

TEST_F(TimerTest, JustWorks) {
  const auto tp = std::make_shared<ThreadPool>(1);
  tp->start();

  Timer timer{watcher, tp};

  bool timer_fired{false};
  std::mutex mutex;
  std::condition_variable cv;

  constexpr auto eps = 5ms;

  timer.schedule(200ms, [&] {
    std::lock_guard lock(mutex);
    timer_fired = true;
    cv.notify_one();
  });

  {
    std::unique_lock lock(mutex);
    ASSERT_TRUE(cv.wait_for(lock, 200ms + eps, [&] { return timer_fired; }));
    EXPECT_TRUE(timer_fired);
  }
}

TEST_F(TimerTest, DoNotBlockEventLoop) {
  const auto tp = std::make_shared<ThreadPool>(2);
  tp->start();

  Timer timer{watcher, tp};

  std::latch latch{2};
  std::chrono::steady_clock::time_point timer1_trigger_time;
  std::chrono::steady_clock::time_point timer2_trigger_time;

  timer.schedule(200ms, [&] {
    timer1_trigger_time = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(150ms);  // Simulate expensive work
    latch.count_down();
  });

  timer.schedule(200ms, [&] {
    timer2_trigger_time = std::chrono::steady_clock::now();
    latch.count_down();
  });

  latch.wait();
  EXPECT_NEAR(
      duration_cast<milliseconds>(timer1_trigger_time.time_since_epoch())
          .count(),
      duration_cast<milliseconds>(timer2_trigger_time.time_since_epoch())
          .count(),
      10);
}
