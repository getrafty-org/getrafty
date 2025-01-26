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
