#include "ephemeral_channel.hpp"
#include <gtest/gtest.h>
#include <latch>
#include <random>
#include <thread>
#include <vector>

using namespace getrafty::rpc::io;
using namespace std::chrono_literals;
using getrafty::wheels::concurrent::ThreadPool;

struct EphemeralChannelTest : testing::Test {
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
