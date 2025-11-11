#include <thread_pool.hpp>
#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <wait_group.hpp>

using namespace std::chrono_literals;
using namespace getrafty::concurrent;

TEST(ThreadPoolTest, JustWorks) {
  WaitGroup wg;
  ThreadPool tp{1};

  tp.start();

  wg.add(1);
  tp.submit([&] {
    std::cout << "Just works" << std::endl;
    wg.done();
  });

  wg.wait();
  tp.stop();
}

TEST(ThreadPoolTest, MultiWait) {
  ThreadPool tp{4};

  tp.start();

  for (size_t i = 0; i < 3; ++i) {
    WaitGroup wg;
    std::atomic<bool> done{false};
    wg.add(1);
    tp.submit([&] {
      std::this_thread::sleep_for(100ms);
      done = true;
      wg.done();
    });

    wg.wait();

    ASSERT_TRUE(done);
  }

  tp.stop();

  ASSERT_TRUE(true);
}

TEST(ThreadPoolTest, Submit) {
  WaitGroup wg;
  ThreadPool tp{4};

  tp.start();

  constexpr size_t kTasks = 100;

  std::atomic<size_t> tasks{0};

  for (size_t i = 0; i < kTasks; ++i) {
    wg.add(1);
    tp.submit([&] {
      ++tasks;
      wg.done();
    });
  }

  wg.wait();
  tp.stop();

  ASSERT_EQ(tasks.load(), kTasks);
}

TEST(ThreadPoolTest, DoNotBurnCPU) {
  struct StopWatch {
    StopWatch() : start_ts_(std::clock()){};

    [[nodiscard]] auto spent() const {
      const size_t clocks = std::clock() - start_ts_;
      return std::chrono::microseconds((clocks * 1000000) / CLOCKS_PER_SEC);
    }
    std::clock_t start_ts_;
  };

  WaitGroup wg;
  ThreadPool tp{4};

  tp.start();

  // Warmup
  for (size_t i = 0; i < 4; ++i) {
    wg.add(1);
    tp.submit([&] {
      std::this_thread::sleep_for(100ms);
      wg.done();
    });
  }

  const StopWatch sw;

  wg.wait();
  tp.stop();

  ASSERT_TRUE(sw.spent() < 100ms);
}

TEST(ThreadPoolTest, Lifetime) {
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
