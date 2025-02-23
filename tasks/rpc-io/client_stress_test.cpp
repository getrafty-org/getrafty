#include <folly/coro/Collect.h>
#include <folly/experimental/coro/GtestHelpers.h>
#include <folly/futures/Future.h>
#include <folly/init/Init.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <cstdint>
#include <latch>
#include <memory>
#include <numeric>
#include <random>
#include "client.hpp"

using namespace std::chrono;
using namespace getrafty::rpc;
using namespace getrafty::rpc::io;
using namespace std::chrono_literals;

using ::testing::_;
using testing::AtLeast;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;

class MockMessage : public IMessage {
 public:
  MOCK_METHOD(void, setBody, (const std::string& data), (override));
  MOCK_METHOD(std::string&, getBody, (), (const, override));

  MOCK_METHOD(void, setMethod, (const std::string& method), (override));
  MOCK_METHOD(std::string&, getMethod, (), (const, override));

  MOCK_METHOD(void, setSequenceId, (uint64_t value), (override));
  MOCK_METHOD(uint64_t, getSequenceId, (), (const, override));

  MOCK_METHOD(void, setProtocol, (const std::string& protocol), (override));
  MOCK_METHOD(std::string&, getProtocol, (), (const, override));

  MOCK_METHOD(void, setErrorCode, (RpcError::Code), (override));
  MOCK_METHOD(RpcError::Code, getErrorCode, (), (const, override));

  MOCK_METHOD(std::shared_ptr<getrafty::rpc::io::IMessage>,
              constructFromCurrent, (), (override));
};

class MockChannel : public IAsyncChannel {
 public:
  MOCK_METHOD(MessagePtr, createMessage, (), (override));
  MOCK_METHOD(void, sendMessage,
              (AsyncCallback&&, MessagePtr, std::chrono::milliseconds),
              (override));
  MOCK_METHOD(void, recvMessage, (AsyncCallback&&, std::chrono::milliseconds),
              (override));
};

class SomeObject : public ISerializable {
 public:
  SomeObject() = default;

  void serialize(IMessage& m) const override { m.setBody(data_); }
  void deserialize(IMessage& m) override { data_ = m.getBody(); }
  std::string& data() { return data_; }

 private:
  std::string data_;
};

class ClientStressTest : public testing::Test {
 protected:
  std::shared_ptr<StrictMock<MockChannel>> mock_channel_{
      std::make_shared<StrictMock<MockChannel>>()};

  std::unique_ptr<Client> client_;

  void SetUp() override { client_ = std::make_unique<Client>(mock_channel_); }
};

CO_TEST_F(ClientStressTest, ConcurrentCallsWithRandomDelay) {
  constexpr auto NUM_CONCURRENT = 50;

  // 1) Prepare vectors for requests, responses, data, and callbacks
  std::vector<std::shared_ptr<MockMessage>> request_msgs;
  std::vector<std::shared_ptr<MockMessage>> response_msgs;
  std::vector<std::string> req_data;
  std::vector<std::string> resp_data;
  std::vector<AsyncCallback> saved_recv_callbacks;
  std::latch callback_latch{NUM_CONCURRENT};

  request_msgs.reserve(NUM_CONCURRENT);
  response_msgs.reserve(NUM_CONCURRENT);
  req_data.reserve(NUM_CONCURRENT);
  resp_data.reserve(NUM_CONCURRENT);
  saved_recv_callbacks.reserve(NUM_CONCURRENT);

  for (int i = 0; i < NUM_CONCURRENT; i++) {
    request_msgs.push_back(std::make_shared<MockMessage>());
    response_msgs.push_back(std::make_shared<MockMessage>());
    req_data.push_back("request" + std::to_string(i));
    resp_data.push_back("response" + std::to_string(i));
  }

  // 2) Mock: createMessage
  int msg_counter = 0;
  EXPECT_CALL(*mock_channel_, createMessage())
      .Times(NUM_CONCURRENT)
      .WillRepeatedly(Invoke([&msg_counter, &request_msgs]() {
        return request_msgs[msg_counter++];
      }));

  // 3) Minimal expectations for all request messages
  for (const auto& msg : request_msgs) {
    EXPECT_CALL(*msg, setBody(_)).Times(AtLeast(0));
    EXPECT_CALL(*msg, setMethod("testMethod")).Times(AtLeast(0));
    EXPECT_CALL(*msg, setSequenceId(_)).Times(AtLeast(0));
  }

  // 4) Setup response message expectations
  for (int i = 0; i < NUM_CONCURRENT; i++) {
    EXPECT_CALL(*response_msgs[i], getErrorCode())
        .WillOnce(Return(RpcError::Code::OK));
    EXPECT_CALL(*response_msgs[i], getSequenceId())
        .WillOnce(Return(i));
    EXPECT_CALL(*response_msgs[i], getBody())
        .WillOnce(ReturnRef(resp_data[i]));
  }

  // 5) Mock: sendMessage & recvMessage
  EXPECT_CALL(*mock_channel_, sendMessage(_, _, _))
      .Times(NUM_CONCURRENT)
      .WillRepeatedly(Invoke([](AsyncCallback&& cb, auto, auto) {
        Result result{.status = IOStatus::OK, .message = nullptr};
        cb(result);
      }));

  EXPECT_CALL(*mock_channel_, recvMessage(_, _))
      .Times(NUM_CONCURRENT)
      .WillRepeatedly(Invoke([&saved_recv_callbacks, &callback_latch](AsyncCallback cob, auto) {
        saved_recv_callbacks.push_back(std::move(cob));
        callback_latch.count_down();
      }));

  // 6) Build a vector of tasks, each performing one RPC call
  std::vector<folly::coro::Task<SomeObject>> tasks;
  tasks.reserve(NUM_CONCURRENT);

  for (int i = 0; i < NUM_CONCURRENT; i++) {
    tasks.push_back([this, i, &req_data]() -> folly::coro::Task<SomeObject> {
      SomeObject request;
      request.data() = req_data[i];
      co_return co_await client_->call<SomeObject, SomeObject>("testMethod", request);
    }());
  }

  // 7) Launch all tasks concurrently using collectAllRange
  auto collect_task =
      collectAllRange(std::move(tasks))
          .scheduleOn(folly::getGlobalCPUExecutor());

  // Convert to a SemiFuture for later co_await
  auto semi = std::move(collect_task).start();

  // 8) Wait until we have all recvMessage callbacks
  callback_latch.wait();

  // 9) Shuffle the response order to simulate random delays
  std::vector<int> response_order(NUM_CONCURRENT);
  std::iota(response_order.begin(), response_order.end(), 0);

  std::random_device rd;
  std::mt19937 g(rd());
  std::ranges::shuffle(response_order, g);

  // 10) Invoke each callback in random order
  for (int idx : response_order) {
    saved_recv_callbacks[idx]({IOStatus::OK, response_msgs[idx]});
  }

  // 11) Gather results
  auto results = co_await std::move(semi);

  // 12) Verify the results match the expected responses
  for (size_t i = 0; i < results.size(); i++) {
    EXPECT_EQ(results[i].data(), resp_data[i]);
  }
}


int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  folly::Init init(&argc, &argv);
  return RUN_ALL_TESTS();
}
