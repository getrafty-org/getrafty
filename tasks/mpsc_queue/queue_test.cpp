#include "queue.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <latch>
#include <memory>
#include <thread>
#include <vector>

using namespace getrafty::concurrent;

TEST(QueueTest, BasicPushTake) {
  Queue<int> queue;

  queue.push(42);
  auto result = queue.tryTake();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 42);
}

TEST(QueueTest, EmptyQueue) {
  Queue<int> queue;

  auto result = queue.tryTake();

  EXPECT_FALSE(result.has_value());
}

TEST(QueueTest, MultiplePushTake) {
  Queue<int> queue;

  queue.push(1);
  queue.push(2);
  queue.push(3);

  auto r1 = queue.tryTake();
  auto r2 = queue.tryTake();
  auto r3 = queue.tryTake();
  auto r4 = queue.tryTake();

  ASSERT_TRUE(r1.has_value());
  EXPECT_EQ(r1.value(), 1);

  ASSERT_TRUE(r2.has_value());
  EXPECT_EQ(r2.value(), 2);

  ASSERT_TRUE(r3.has_value());
  EXPECT_EQ(r3.value(), 3);

  EXPECT_FALSE(r4.has_value());
}

TEST(QueueTest, MoveOnlyType) {
  Queue<std::unique_ptr<int>> queue;

  queue.push(std::make_unique<int>(42));

  auto result = queue.tryTake();

  ASSERT_TRUE(result.has_value());
  ASSERT_NE(result.value(), nullptr);
  EXPECT_EQ(*result.value(), 42);
}

TEST(QueueTest, MultipleProducersSingleConsumer) {
  Queue<int> queue;

  constexpr int kNumProducers = 4;
  constexpr int kItemsPerProducer = 1000;

  std::vector<std::thread> producers;
  producers.reserve(kNumProducers);

  std::latch start_latch(kNumProducers + 1);

  for (int p = 0; p < kNumProducers; ++p) {
    producers.emplace_back([&queue, &start_latch, p]() {
      start_latch.count_down();
      start_latch.wait();

      for (int i = 0; i < kItemsPerProducer; ++i) {
        queue.push(p * kItemsPerProducer + i);
      }
    });
  }

  start_latch.count_down();
  start_latch.wait();

  std::vector<int> consumed;
  consumed.reserve(kNumProducers * kItemsPerProducer);

  for (auto& producer : producers) {
    producer.join();
  }

  while (auto item = queue.tryTake()) {
    consumed.push_back(item.value());
  }

  EXPECT_EQ(consumed.size(), kNumProducers * kItemsPerProducer);

  std::sort(consumed.begin(), consumed.end());
  for (size_t i = 0; i < consumed.size(); ++i) {
    EXPECT_EQ(consumed[i], static_cast<int>(i));
  }
}

TEST(QueueTest, ConcurrentPushTake) {
  Queue<int> queue;

  constexpr int kNumProducers = 4;
  constexpr int kItemsPerProducer = 1000;

  std::atomic<bool> consumer_running{true};
  std::atomic<int> consumed_count{0};

  std::vector<std::thread> producers;
  producers.reserve(kNumProducers);

  std::latch start_latch(kNumProducers + 2);

  std::thread consumer([&]() {
    start_latch.count_down();
    start_latch.wait();

    while (consumer_running.load(std::memory_order_acquire)) {
      if (auto item = queue.tryTake()) {
        consumed_count.fetch_add(1, std::memory_order_relaxed);
      }
    }

    // Drain remaining items
    while (auto item = queue.tryTake()) {
      consumed_count.fetch_add(1, std::memory_order_relaxed);
    }
  });

  for (int p = 0; p < kNumProducers; ++p) {
    producers.emplace_back([&queue, &start_latch]() {
      start_latch.count_down();
      start_latch.wait();

      for (int i = 0; i < kItemsPerProducer; ++i) {
        queue.push(i);
      }
    });
  }

  start_latch.count_down();
  start_latch.wait();

  for (auto& producer : producers) {
    producer.join();
  }

  consumer_running.store(false, std::memory_order_release);
  consumer.join();

  EXPECT_EQ(consumed_count.load(), kNumProducers * kItemsPerProducer);
}

TEST(QueueTest, StressTest) {
  Queue<int> queue;

  constexpr int kNumProducers = 8;
  constexpr int kItemsPerProducer = 10000;

  std::atomic<bool> consumer_running{true};
  std::atomic<int64_t> consumed_sum{0};
  std::atomic<int> consumed_count{0};

  std::vector<std::thread> producers;
  producers.reserve(kNumProducers);

  std::latch start_latch(kNumProducers + 2);

  std::thread consumer([&]() {
    start_latch.count_down();
    start_latch.wait();

    while (consumer_running.load(std::memory_order_acquire)) {
      if (auto item = queue.tryTake()) {
        consumed_sum.fetch_add(item.value(), std::memory_order_relaxed);
        consumed_count.fetch_add(1, std::memory_order_relaxed);
      }
    }

    while (auto item = queue.tryTake()) {
      consumed_sum.fetch_add(item.value(), std::memory_order_relaxed);
      consumed_count.fetch_add(1, std::memory_order_relaxed);
    }
  });

  int64_t expected_sum = 0;
  for (int p = 0; p < kNumProducers; ++p) {
    producers.emplace_back([&queue, &start_latch]() {
      start_latch.count_down();
      start_latch.wait();

      for (int i = 0; i < kItemsPerProducer; ++i) {
        queue.push(i);
      }
    });

    for (int i = 0; i < kItemsPerProducer; ++i) {
      expected_sum += i;
    }
  }

  start_latch.count_down();
  start_latch.wait();

  for (auto& producer : producers) {
    producer.join();
  }

  consumer_running.store(false, std::memory_order_release);
  consumer.join();

  EXPECT_EQ(consumed_count.load(), kNumProducers * kItemsPerProducer);
  EXPECT_EQ(consumed_sum.load(), expected_sum);
}

TEST(QueueTest, LargeItems) {
  Queue<std::vector<int>> queue;

  std::vector<int> large_item(10000, 42);
  queue.push(large_item);

  auto result = queue.tryTake();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value().size(), 10000);
  EXPECT_EQ(result.value()[0], 42);
  EXPECT_EQ(result.value()[9999], 42);
}

TEST(QueueTest, InterleavedPushTake) {
  Queue<int> queue;

  for (int i = 0; i < 100; ++i) {
    queue.push(i);
    auto result = queue.tryTake();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), i);
  }

  auto empty = queue.tryTake();
  EXPECT_FALSE(empty.has_value());
}

TEST(QueueTest, BurstPattern) {
  Queue<int> queue;

  // Burst of pushes
  for (int i = 0; i < 1000; ++i) {
    queue.push(i);
  }

  // Burst of takes
  for (int i = 0; i < 1000; ++i) {
    auto result = queue.tryTake();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), i);
  }

  auto empty = queue.tryTake();
  EXPECT_FALSE(empty.has_value());
}

TEST(QueueTest, StringType) {
  Queue<std::string> queue;

  queue.push("hello");
  queue.push("world");

  auto r1 = queue.tryTake();
  auto r2 = queue.tryTake();

  ASSERT_TRUE(r1.has_value());
  EXPECT_EQ(r1.value(), "hello");

  ASSERT_TRUE(r2.has_value());
  EXPECT_EQ(r2.value(), "world");
}

TEST(QueueTest, CustomStruct) {
  struct Data {
    int id;
    std::string name;
  };

  Queue<Data> queue;

  queue.push(Data{1, "first"});
  queue.push(Data{2, "second"});

  auto r1 = queue.tryTake();
  auto r2 = queue.tryTake();

  ASSERT_TRUE(r1.has_value());
  EXPECT_EQ(r1.value().id, 1);
  EXPECT_EQ(r1.value().name, "first");

  ASSERT_TRUE(r2.has_value());
  EXPECT_EQ(r2.value().id, 2);
  EXPECT_EQ(r2.value().name, "second");
}

TEST(QueueTest, RapidProducerSlowConsumer) {
  Queue<int> queue;

  std::atomic<bool> producer_done{false};
  constexpr int kNumItems = 10000;

  std::thread producer([&]() {
    for (int i = 0; i < kNumItems; ++i) {
      queue.push(i);
    }
    producer_done.store(true, std::memory_order_release);
  });

  std::vector<int> consumed;
  consumed.reserve(kNumItems);

  while (!producer_done.load(std::memory_order_acquire) ||
         consumed.size() < kNumItems) {
    if (auto item = queue.tryTake()) {
      consumed.push_back(item.value());
      std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
  }

  producer.join();

  EXPECT_EQ(consumed.size(), kNumItems);
  for (size_t i = 0; i < consumed.size(); ++i) {
    EXPECT_EQ(consumed[i], static_cast<int>(i));
  }
}

TEST(QueueTest, MultipleQueues) {
  Queue<int> queue1;
  Queue<int> queue2;

  queue1.push(1);
  queue2.push(2);

  auto r1 = queue1.tryTake();
  auto r2 = queue2.tryTake();

  ASSERT_TRUE(r1.has_value());
  EXPECT_EQ(r1.value(), 1);

  ASSERT_TRUE(r2.has_value());
  EXPECT_EQ(r2.value(), 2);

  EXPECT_FALSE(queue1.tryTake().has_value());
  EXPECT_FALSE(queue2.tryTake().has_value());
}
