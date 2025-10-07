#include <queue.hpp>
#include <gtest/gtest.h>
#include <wait_group.hpp>

#include <sys/resource.h>

#include <atomic>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace getrafty::concurrent;

TEST(QueueTest, JustWorks) {
  BlockingMPMCQueue<int> queue;

  queue.put(42);

  ASSERT_EQ(queue.take(), 42);
}

TEST(QueueTest, FIFO) {
  BlockingMPMCQueue<int> queue;

  for (int i = 0; i < 10; ++i) {
    queue.put(i);
  }

  for (int i = 0; i < 10; ++i) {
    ASSERT_EQ(queue.take(), i);
  }
}

TEST(QueueTest, BlockingBehavior) {
  BlockingMPMCQueue<int> queue;
  std::atomic<bool> taken{false};

  std::thread consumer([&] {
    const int value = queue.take();
    taken = true;
    EXPECT_EQ(value, 123);
  });

  std::this_thread::sleep_for(100ms);
  ASSERT_FALSE(taken);

  queue.put(123);

  consumer.join();
  ASSERT_TRUE(taken);
}

TEST(QueueTest, SingleProducerSingleConsumer) {
  BlockingMPMCQueue<int> queue;
  constexpr size_t kItems = 1000;
  std::atomic<size_t> consumed{0};

  std::thread producer([&] {
    for (size_t i = 0; i < kItems; ++i) {
      queue.put(static_cast<int>(i));
    }
  });

  std::thread consumer([&] {
    for (size_t i = 0; i < kItems; ++i) {
      const int value = queue.take();
      EXPECT_EQ(value, static_cast<int>(i));
      ++consumed;
    }
  });

  producer.join();
  consumer.join();

  ASSERT_EQ(consumed.load(), kItems);
}

TEST(QueueTest, MultipleProducersSingleConsumer) {
  BlockingMPMCQueue<int> queue;
  constexpr size_t kProducers = 4;
  constexpr size_t kItemsPerProducer = 100;
  constexpr size_t kTotalItems = kProducers * kItemsPerProducer;

  std::vector<std::thread> producers;
  std::atomic<size_t> consumed{0};

  for (size_t p = 0; p < kProducers; ++p) {
    producers.emplace_back([&, p] {
      for (size_t i = 0; i < kItemsPerProducer; ++i) {
        queue.put(static_cast<int>(p * kItemsPerProducer + i));
      }
    });
  }

  std::thread consumer([&] {
    std::vector<int> received;
    for (size_t i = 0; i < kTotalItems; ++i) {
      received.push_back(queue.take());
      ++consumed;
    }

    std::sort(received.begin(), received.end());
    for (size_t i = 0; i < kTotalItems; ++i) {
      EXPECT_EQ(received[i], static_cast<int>(i));
    }
  });

  for (auto& producer : producers) {
    producer.join();
  }
  consumer.join();

  ASSERT_EQ(consumed.load(), kTotalItems);
}

TEST(QueueTest, SingleProducerMultipleConsumers) {
  BlockingMPMCQueue<int> queue;
  constexpr size_t kConsumers = 4;
  constexpr size_t kItemsPerConsumer = 100;
  constexpr size_t kTotalItems = kConsumers * kItemsPerConsumer;

  std::atomic<size_t> consumed{0};
  std::vector<std::thread> consumers;
  std::mutex result_mutex;
  std::vector<int> all_consumed;

  for (size_t c = 0; c < kConsumers; ++c) {
    consumers.emplace_back([&] {
      std::vector<int> local;
      for (size_t i = 0; i < kItemsPerConsumer; ++i) {
        const int value = queue.take();
        local.push_back(value);
        ++consumed;
      }

      std::lock_guard lock(result_mutex);
      all_consumed.insert(all_consumed.end(), local.begin(), local.end());
    });
  }

  std::thread producer([&] {
    for (size_t i = 0; i < kTotalItems; ++i) {
      queue.put(static_cast<int>(i));
    }
  });

  producer.join();
  for (auto& consumer : consumers) {
    consumer.join();
  }

  ASSERT_EQ(consumed.load(), kTotalItems);
  ASSERT_EQ(all_consumed.size(), kTotalItems);

  std::sort(all_consumed.begin(), all_consumed.end());
  for (size_t i = 0; i < kTotalItems; ++i) {
    EXPECT_EQ(all_consumed[i], static_cast<int>(i));
  }
}

TEST(QueueTest, MultipleProducersMultipleConsumers) {
  BlockingMPMCQueue<int> queue;
  constexpr size_t kProducers = 3;
  constexpr size_t kConsumers = 3;
  constexpr size_t kItemsPerProducer = 100;
  constexpr size_t kItemsPerConsumer = 100;
  constexpr size_t kTotalItems = kProducers * kItemsPerProducer;

  std::vector<std::thread> producers;
  std::vector<std::thread> consumers;
  std::atomic<size_t> consumed{0};
  std::mutex result_mutex;
  std::vector<int> all_consumed;

  for (size_t p = 0; p < kProducers; ++p) {
    producers.emplace_back([&, p] {
      for (size_t i = 0; i < kItemsPerProducer; ++i) {
        queue.put(static_cast<int>(p * kItemsPerProducer + i));
      }
    });
  }

  for (size_t c = 0; c < kConsumers; ++c) {
    consumers.emplace_back([&] {
      std::vector<int> local;
      for (size_t i = 0; i < kItemsPerConsumer; ++i) {
        const int value = queue.take();
        local.push_back(value);
        ++consumed;
      }

      std::lock_guard lock(result_mutex);
      all_consumed.insert(all_consumed.end(), local.begin(), local.end());
    });
  }

  for (auto& producer : producers) {
    producer.join();
  }
  for (auto& consumer : consumers) {
    consumer.join();
  }

  ASSERT_EQ(consumed.load(), kTotalItems);
  ASSERT_EQ(all_consumed.size(), kTotalItems);

  // Verify all items were consumed exactly once
  std::sort(all_consumed.begin(), all_consumed.end());
  for (size_t i = 0; i < kTotalItems; ++i) {
    EXPECT_EQ(all_consumed[i], static_cast<int>(i));
  }
}

TEST(QueueTest, StressTest) {
  BlockingMPMCQueue<int> queue;
  constexpr size_t kProducers = 8;
  constexpr size_t kConsumers = 8;
  constexpr size_t kItemsPerProducer = 500;
  constexpr size_t kItemsPerConsumer = 500;
  constexpr size_t kTotalItems = kProducers * kItemsPerProducer;

  std::vector<std::thread> producers;
  std::vector<std::thread> consumers;
  std::atomic<size_t> consumed{0};

  for (size_t p = 0; p < kProducers; ++p) {
    producers.emplace_back([&, p] {
      for (size_t i = 0; i < kItemsPerProducer; ++i) {
        queue.put(static_cast<int>(p * kItemsPerProducer + i));
        if (i % 10 == 0) {
          std::this_thread::yield();
        }
      }
    });
  }

  for (size_t c = 0; c < kConsumers; ++c) {
    consumers.emplace_back([&] {
      for (size_t i = 0; i < kItemsPerConsumer; ++i) {
        queue.take();
        ++consumed;
        if (i % 10 == 0) {
          std::this_thread::yield();
        }
      }
    });
  }

  for (auto& producer : producers) {
    producer.join();
  }
  for (auto& consumer : consumers) {
    consumer.join();
  }

  ASSERT_EQ(consumed.load(), kTotalItems);
}

TEST(QueueTest, DoesNotBusyWait) {
  BlockingMPMCQueue<int> queue;

  std::thread consumer([&] {
    queue.take();
  });

  std::this_thread::sleep_for(100ms);

  struct rusage usage_before, usage_after;
  getrusage(RUSAGE_SELF, &usage_before);

  std::this_thread::sleep_for(5000ms);

  getrusage(RUSAGE_SELF, &usage_after);

  const auto user_time_us =
      (usage_after.ru_utime.tv_sec - usage_before.ru_utime.tv_sec) * 1000000 +
      (usage_after.ru_utime.tv_usec - usage_before.ru_utime.tv_usec);

  queue.put(1);
  consumer.join();

  ASSERT_LT(user_time_us, 30000);
}

TEST(QueueTest, MoveOnlyType) {
  struct MoveOnly {
    explicit MoveOnly(int v) : value(v) {}
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;

    int value;
  };

  BlockingMPMCQueue<MoveOnly> queue;

  queue.put(MoveOnly(42));

  MoveOnly result = queue.take();
  ASSERT_EQ(result.value, 42);
}
