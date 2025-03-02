#include <folly/experimental/coro/GtestHelpers.h>
#include <folly/experimental/coro/Task.h>
#include <gtest/gtest.h>
#include <chrono>
#include "folly/init/Init.h"

#include "queue.hpp"

#include "folly/Random.h"
#include "folly/coro/Collect.h"
#include "folly/coro/Sleep.h"

using namespace std::chrono_literals;
using namespace getrafty::wheels::concurrent::coro;

class UnboundedBlockingQueueTest : public ::testing::Test {};

// Test that a value already in the queue is taken immediately.
CO_TEST_F(UnboundedBlockingQueueTest, ImmediateTake) {
  UnboundedBlockingQueue<int> queue;
  queue.put(42);
  const auto value = co_await queue.take();
  EXPECT_EQ(value, 42);
  co_return;
}

// Test that a suspended take resumes when a value is put.
CO_TEST_F(UnboundedBlockingQueueTest, SuspendAndResumeTake) {
  UnboundedBlockingQueue<int> queue;
  auto task = queue.take();
  // At this point the coroutine is suspended waiting for a value.
  queue.put(100);
  const auto value = co_await std::move(task);
  EXPECT_EQ(value, 100);
  co_return;
}

// Test that multiple takes and puts behave in FIFO order.
CO_TEST_F(UnboundedBlockingQueueTest, MultipleTakesAndPuts) {
  UnboundedBlockingQueue<int> queue;

  auto task1 = queue.take();
  auto task2 = queue.take();
  auto task3 = queue.take();

  queue.put(10);
  queue.put(20);
  queue.put(30);

  const auto value1 = co_await std::move(task1);
  const auto value2 = co_await std::move(task2);
  const auto value3 = co_await std::move(task3);

  EXPECT_EQ(value1, 10);
  EXPECT_EQ(value2, 20);
  EXPECT_EQ(value3, 30);
  co_return;
}

CO_TEST_F(UnboundedBlockingQueueTest, MixedContextPutAndTake) {
  UnboundedBlockingQueue<int> queue;

  // Test that put() called from a non-coroutine context (a separate thread)
  // unblocks a suspended coroutine take().
  std::thread producer([&queue]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    queue.put(77);
  });

  const auto value = co_await queue.take();
  EXPECT_EQ(value, 77);
  producer.join();
  co_return;
}

CO_TEST_F(UnboundedBlockingQueueTest, StressTest) {
  constexpr uint32_t kProducers = 10;
  constexpr uint32_t kConsumers = 10;
  constexpr uint32_t kItemsPerProducer = 1000;
  constexpr uint32_t kTotalItems = kProducers * kItemsPerProducer;
  constexpr uint32_t kItemsPerConsumer = kTotalItems / kConsumers;

  UnboundedBlockingQueue<uint32_t> queue;

  folly::Synchronized<std::vector<uint32_t>> produced_items_freq;
  folly::Synchronized<std::vector<uint32_t>> consumed_items_freq;
  produced_items_freq.withWLock([&](auto &vec) { vec.resize(kTotalItems, 0); });
  consumed_items_freq.withWLock([&](auto &vec) { vec.resize(kTotalItems, 0); });


  // Use one vector for both producers and consumers.
  std::vector<folly::coro::Task<>> tasks;
  tasks.reserve(kProducers + kConsumers);

  // Spawn producer tasks.
  for (int i = 0; i < kProducers; ++i) {
    tasks.emplace_back(folly::coro::co_invoke(
        [&queue, &produced_items_freq]() -> folly::coro::Task<> {
          for (int j = 0; j < kItemsPerProducer; ++j) {
            const auto value = folly::Random::rand32(kTotalItems);
            produced_items_freq.withWLock([&](auto lock) { ++lock[value]; });
            queue.put(value);

            if (value % 31 == 0) {
              co_await folly::coro::sleep(10ms);
            }
          }
          co_return;
        }));
  }

  // Spawn consumer tasks.
  for (int i = 0; i < kConsumers; ++i) {
    tasks.push_back(folly::coro::co_invoke(
        [&queue, &consumed_items_freq]() -> folly::coro::Task<> {
          for (int j = 0; j < kItemsPerConsumer; ++j) {
            const auto value = co_await std::move(queue.take());
            consumed_items_freq.withWLock([&](auto lock) { ++lock[value]; });
            if (value % 31 == 0) {
              co_await folly::coro::sleep(10ms);
            }
          }
          co_return;
        }));
  }

  // Await all tasks concurrently.
  co_await collectAllRange(std::move(tasks));

  const auto consumed_items_freq_ptr = consumed_items_freq.rlock();
  const auto produced_items_freq_ptr = produced_items_freq.rlock();
  for(auto i =0; i < kTotalItems; ++i) {
    EXPECT_EQ(consumed_items_freq_ptr->at(i), produced_items_freq_ptr->at(i));
  }

  co_return;
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  folly::Init init(&argc, &argv);
  return RUN_ALL_TESTS();
}
