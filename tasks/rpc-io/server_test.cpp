#include <folly/coro/BlockingWait.h>
#include <folly/experimental/coro/GtestHelpers.h>
#include <folly/init/Init.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

#include "client.hpp"
#include "ephemeral_transport.hpp"
#include "server.hpp"

#include "folly/MPMCQueue.h"

using namespace std::chrono_literals;
using namespace getrafty::rpc;
using namespace getrafty::rpc::io;
using ::testing::_;

class SomeObject : public ISerializable {
 public:
  SomeObject() = default;
  void serialize(IMessage& m) const override {
    m.setBody(data_);
  }
  void deserialize(IMessage& m) override {
    data_ = m.getBody();
  }
  std::string& data() { return data_; }
  [[nodiscard]] const std::string& data() const { return data_; }

 private:
  std::string data_;
};

class EphemeralListener final : public IListener {
 public:
  explicit EphemeralListener(const uint16_t address)
      : address_(address),
        tp_(std::make_shared<ThreadPool>(std::thread::hardware_concurrency())) {
    tp_->start();
    const auto channel = EphemeralChannel::create(address_, tp_);
    if (const auto p = dynamic_cast<EphemeralChannel*>(channel.get())) {
      p->setOnCloseCallback(
          std::make_shared<std::function<void(std::shared_ptr<IClientSocket>)>>(
              [this](const std::shared_ptr<IClientSocket>& chan) {
                chan->open();
                queue_.write(chan);
              }));
    }
    channel->open();
    queue_.write(channel);
  }

  ~EphemeralListener() override {
    if (std::shared_ptr<IClientSocket> ch; queue_.read(ch)) {
      ch->close();
    }
    tp_->stop();
  }

  folly::coro::Task<std::shared_ptr<IClientSocket>> accept() override {
    std::shared_ptr<IClientSocket> ch;
    queue_.blockingRead(ch);
    assert(ch);
    co_return ch;
  }

 private:
  uint16_t address_;
  std::shared_ptr<ThreadPool> tp_;
  folly::MPMCQueue<std::shared_ptr<IClientSocket>> queue_{1};
};

class ServerTest : public ::testing::Test {
 public:
  void SetUp() override {
    tp_ = std::make_shared<ThreadPool>(std::thread::hardware_concurrency());
    listener_ = std::make_shared<EphemeralListener>(12345);
    server_ = std::make_shared<Server>(listener_);
    server_->addHandler<SomeObject, SomeObject>(
        "echo", [](const SomeObject& req) -> folly::coro::Task<SomeObject> {
          SomeObject resp;
          resp.data() = req.data();
          co_return resp;
        });

    client_ = std::make_shared<Client>(EphemeralChannel::create(12345, tp_));

    tp_->start();
  }

  void TearDown() override { tp_->stop(); }

 protected:
  std::shared_ptr<ThreadPool> tp_;
  std::shared_ptr<Server> server_;
  std::shared_ptr<Client> client_;
  std::shared_ptr<EphemeralListener> listener_;
};

CO_TEST_F(ServerTest, EchoWorks) {
  co_await server_->start();

  SomeObject request{};
  request.data() = "1";
  auto response =
      co_await client_->call<SomeObject, SomeObject>("echo", request);
  EXPECT_EQ(response.data(), "1");

  std::this_thread::sleep_for(5s);
  co_await server_->stop();


}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  folly::Init init(&argc, &argv);
  return RUN_ALL_TESTS();
}
