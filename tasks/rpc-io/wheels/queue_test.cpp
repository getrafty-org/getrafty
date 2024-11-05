#include "queue.hpp"

#include <latch>
#include <gtest/gtest.h>
#include <thread>

using namespace getrafty::wheels::concurrent;

TEST(QueueTest, JustWorks) {
    UnboundedBlockingQueue<int> q;

    q.put(1);
    q.put(2);
    q.put(3);

    EXPECT_EQ(q.take(), 1);
    EXPECT_EQ(q.take(), 2);
    EXPECT_EQ(q.take(), 3);
}

TEST(QueueTest, BlockIfEmpty) {
    UnboundedBlockingQueue<int> q;
    std::latch latch{1};
    std::atomic<bool> done;
    std::thread t{
        [&]() {
            latch.count_down();
            EXPECT_EQ(q.take(), 5);
            done = true;
        }
    };

    latch.wait(); // this is not needed functionally wise however we leave it for better communication of the intention

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_FALSE(done.load());

    q.put(5); // Wakeup
    t.join();

    EXPECT_TRUE(done.load());
}

TEST(QueueTest, Stress) {
    UnboundedBlockingQueue<uint32_t> q;
    constexpr auto kNumThreads = 100;

    std::latch latch{kNumThreads};
    std::vector<std::thread> threads;
    threads.reserve(kNumThreads);
    for (auto i = 0; i < kNumThreads; ++i) {
        threads.emplace_back([&, i] {
            latch.arrive_and_wait();
            for (auto j = 0; j < 3; ++j) {
                q.put(i);
                // const auto &v = q.take();
                // q.put(v);
            }
        });
    }

    uint32_t foo = 0;
    uint32_t bar = 0;
    for (auto i = 0; i < kNumThreads; ++i) {
        foo += 3 * i;
        for (auto j = 0; j < 3; ++j) {
            bar += q.take();
        }
    }

    ASSERT_EQ(foo, bar);

    for (auto &t: threads) {
        t.join();
    }
}
