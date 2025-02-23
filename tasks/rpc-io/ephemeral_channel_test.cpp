#include "ephemeral_channel.hpp"
#include <gtest/gtest.h>
#include <latch>
#include <random>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace getrafty::rpc::io;
using namespace std::chrono_literals;
using getrafty::wheels::concurrent::ThreadPool;

struct EphemeralChannelTest: testing::Test {
  void SetUp() override {
    tp_ = std::make_shared<ThreadPool>(std::thread::hardware_concurrency());
    tp_->start();
  }

  void TearDown() override { tp_->stop(); }

  std::shared_ptr<ThreadPool> tp_;
};

TEST_F(EphemeralChannelTest, JustWorks) {
  constexpr uint16_t channel_id = 42;
  const auto channel1 = EphemeralChannel::create(channel_id, tp_);
  const auto channel2 = EphemeralChannel::create(channel_id, tp_);

  std::latch latch{1};

  channel2->recvMessage(
      [&](const Result& result) {
        ASSERT_EQ(result.status, IOStatus::OK);
        ASSERT_NE(result.message, nullptr);
        EXPECT_EQ(result.message->getBody(), "test");
        latch.count_down();
      },
      1s);

  auto msg = channel1->createMessage();
  msg->setBody("test");
  channel1->sendMessage(
      [](const Result& result) {
        // Verify send succeeded
        ASSERT_EQ(result.status, IOStatus::OK);
      },
      std::move(msg), 1s);

  latch.wait();
}

TEST_F(EphemeralChannelTest, PeerReturnsSockClosed) {
  constexpr uint16_t channel_id = 42;
  const auto channel = EphemeralChannel::create(channel_id, tp_);

  std::latch latch{1};
  auto msg = channel->createMessage();
  channel->sendMessage(
      [&](const Result& result) {
        EXPECT_EQ(result.status, IOStatus::SOCK_CLOSED);
      },
      std::move(msg), 1s);

  latch.count_down();
}

TEST_F(EphemeralChannelTest, RecvMessageTimeout) {
  constexpr uint16_t channel_id = 42;
  const auto channel = EphemeralChannel::create(channel_id, tp_);

  std::atomic callback_called = false;

  channel->recvMessage(
      [&](const Result& result) {
        EXPECT_EQ(result.status, IOStatus::IO_TIMEOUT);
        callback_called = true;
      },
      100ms);

  std::this_thread::sleep_for(200ms);
  EXPECT_TRUE(callback_called);
}

TEST_F(EphemeralChannelTest, ManyMessages) {
  constexpr uint16_t channel_id = 42;
  const auto sender = EphemeralChannel::create(channel_id, tp_);
  const auto receiver = EphemeralChannel::create(channel_id, tp_);

  constexpr int num_messages = 10000;
  std::atomic received_count = 0;
  std::latch latch{num_messages};
  std::mutex received_mutex;
  std::unordered_set<int> received_ids;

  for (int i = 0; i < num_messages; ++i) {
    receiver->recvMessage(
        [&](const Result& result) {
          EXPECT_EQ(result.status, IOStatus::OK);
          EXPECT_NE(result.message, nullptr);
          {
            const auto msg_id = result.message->getSequenceId();
            std::lock_guard lock(received_mutex);
            EXPECT_TRUE(received_ids.insert(msg_id).second);
          }
          received_count.fetch_add(1, std::memory_order_relaxed);
          latch.count_down();
        },
        5s);
  }

  for (int i = 0; i < num_messages; ++i) {
    auto msg = sender->createMessage();
    msg->setSequenceId(i);
    sender->sendMessage(
        [](const Result& result) {
          EXPECT_EQ(result.status, IOStatus::OK);
        },
        std::move(msg), 5s);
  }

  latch.wait();
  EXPECT_EQ(received_count.load(), num_messages);
  EXPECT_EQ(received_ids.size(), num_messages);
}

