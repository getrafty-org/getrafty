#include "thread_pool.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <latch>
#include <thread>

using namespace std::chrono_literals;
using namespace getrafty::wheels::concurrent;

TEST(ThreadPoolTest, JustWorks) {
  WaitGroup wg;
  ThreadPool tp{4};

  tp.start();

  tp.submit([&] {
    wg.add(1);
    std::cout << "Just works" << std::endl;
    wg.done();
  });

  wg.wait();
  tp.stop();
}

TEST(ThreadPoolTest, MultiWait) {
  WaitGroup wg;
  ThreadPool tp{1};

  tp.start();

  for (size_t i = 0; i < 3; ++i) {
    std::atomic<bool> done{false};

    tp.submit([&] {
      wg.add(1);
      std::this_thread::sleep_for(1s);
      done = true;
      wg.done();
    });

    wg.wait();

    ASSERT_TRUE(done);
  }

  tp.stop();
}

TEST(ThreadPoolTest, Submit) {
  WaitGroup wg;
  ThreadPool tp{4};

  tp.start();

  constexpr size_t kTasks = 100;

  std::atomic<size_t> tasks{0};

  for (size_t i = 0; i < kTasks; ++i) {
    tp.submit([&] {
      wg.add(1);
      ++tasks;
      wg.done();
    });
  }

  wg.wait();
  tp.stop();

  ASSERT_EQ(tasks.load(), kTasks);
}

TEST(ThreadPoolTest, DoNotBurnCPU) {
  WaitGroup wg;
  ThreadPool tp{4};

  tp.start();

  // Warmup
  for (size_t i = 0; i < 4; ++i) {
    tp.submit([&] { wg.add(1); std::this_thread::sleep_for(100ms); wg.wait(); });
  }

  struct CpuTimer {
    explicit CpuTimer() : start_ts_(std::clock()) {};

    [[nodiscard]] auto spent() const {
      const size_t clocks = std::clock() - start_ts_;
      return std::chrono::microseconds((clocks * 1000000) / CLOCKS_PER_SEC);
    }

    std::clock_t start_ts_;
  };

  const CpuTimer t;

  std::this_thread::sleep_for(1s);
  
  wg.wait();
  tp.stop();

  ASSERT_TRUE(t.spent() < 100ms);
}

TEST(ThreadPoolTest, Stop) {
  struct Foo {
    Foo() : tp_(ThreadPool{1}) {
      tp_.start();
      tp_.submit([&] { bar(); });
    };

    ~Foo() { tp_.stop(); }

    void bar() {
      std::this_thread::sleep_for(100ms);
      tp_.submit([&] { bar(); });
    }

    ThreadPool tp_;
  };

  { Foo foo; }

  ASSERT_TRUE(true);
}
