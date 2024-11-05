#include <latch>
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "thread_pool.hpp"

using namespace std::chrono_literals;
using namespace getrafty::wheels::concurrent;

TEST(ThreadPoolTest, JustWorks) {
    ThreadPool tp{4};

    tp.start();

    tp.submit([] {
        std::cout << "Hello from thread pool!" << std::endl;
    });

    tp.waitIdle();
    tp.stop();
}

TEST(ThreadPoolTest, MultiWait) {
    ThreadPool tp{1};

    tp.start();

    for (size_t i = 0; i < 3; ++i) {
        std::atomic<bool> done{false};

        tp.submit([&]() {
            std::this_thread::sleep_for(1s);
            done = true;
        });

        tp.waitIdle();

        ASSERT_TRUE(done);
    }

    tp.stop();
}

TEST(ThreadPoolTest, Submit) {
    ThreadPool tp{4};

    tp.start();

    constexpr size_t kTasks = 100;

    std::atomic<size_t> tasks{0};

    for (size_t i = 0; i < kTasks; ++i) {
        tp.submit([&]() {
            ++tasks;
        });
    }

    tp.waitIdle();
    tp.stop();

    ASSERT_EQ(tasks.load(), kTasks);
}

TEST(ThreadPoolTest, DoNotBurnCPU) {
    ThreadPool tp{4};

    tp.start();

    // Warmup
    for (size_t i = 0; i < 4; ++i) {
        tp.submit([&]() {
            std::this_thread::sleep_for(100ms);
        });
    }

    struct CpuTimer {
        explicit CpuTimer(): start_ts_(std::clock()) {
        };

        [[nodiscard]] auto spent() const {
            const size_t clocks = std::clock() - start_ts_;
            return std::chrono::microseconds((clocks * 1'000'000) / CLOCKS_PER_SEC);
        }

        std::clock_t start_ts_;
    };

    const CpuTimer t;


    std::this_thread::sleep_for(1s);

    tp.waitIdle();
    tp.stop();

    ASSERT_TRUE(t.spent() < 100ms);
}

TEST(ThreadPoolTest, Stop) {
    struct Foo {
        Foo() : tp_(ThreadPool{1}) {
            tp_.start();
            tp_.submit([&] { bar(); });
        };

        ~Foo() {
            tp_.stop();
        }

        void bar() {
            std::this_thread::sleep_for(100ms);
            tp_.submit([&] {
                bar();
            });
        }

        ThreadPool tp_;
    };

    {
        Foo foo;
    }

    ASSERT_TRUE(true);
}
