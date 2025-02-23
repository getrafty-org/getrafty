#include "client.hpp"
#include <folly/coro/Collect.h>
#include <folly/experimental/coro/GtestHelpers.h>
#include <folly/futures/Future.h>
#include <folly/init/Init.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <memory>

#include <latch>

using namespace std::chrono;
using namespace getrafty::rpc;
using namespace getrafty::rpc::io;
using namespace std::chrono_literals;

using ::testing::_;
using ::testing::DoAll;
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

  void TearDown() override {}
};

CO_TEST_F(ClientStressTest, JustWorks) {
  // Arrange
  const auto request_msg = std::make_shared<MockMessage>();
  const auto response_msg = std::make_shared<MockMessage>();

  EXPECT_CALL(*mock_channel_, createMessage()).WillOnce(Return(request_msg));

  EXPECT_CALL(*request_msg, setBody("someData"));
  EXPECT_CALL(*request_msg, setMethod("testMethod"));
  EXPECT_CALL(*response_msg, getErrorCode())
      .WillOnce(Return(RpcError::Code::OK));

  bool send_callback_called = false;
  EXPECT_CALL(*mock_channel_, sendMessage(_, _, _))
      .WillOnce(
          Invoke([&send_callback_called](AsyncCallback&& callback, auto, auto) {
            send_callback_called = true;
            callback({IOStatus::OK});
          }));

  bool receive_callback_called = false;
  EXPECT_CALL(*mock_channel_, recvMessage(_, _))
      .WillOnce(Invoke([&receive_callback_called, response_msg](
                           AsyncCallback&& callback, auto) {
        receive_callback_called = true;
        callback({IOStatus::OK, response_msg});
      }));

  EXPECT_CALL(*response_msg, getSequenceId()).WillOnce(Return(0));

  std::string response_body = "otherData";
  EXPECT_CALL(*response_msg, getBody()).WillOnce(ReturnRef(response_body));

  // Act
  SomeObject request;
  request.data() = "someData";
  auto response =
      co_await client_->call<SomeObject, SomeObject>("testMethod", request);

  // Assert
  EXPECT_TRUE(send_callback_called);
  EXPECT_TRUE(receive_callback_called);
  EXPECT_EQ(response.data(), response_body);
}

CO_TEST_F(ClientStressTest, ThrowErrorOnSendTimeout) {
  // Arrange
  const auto msg = std::make_shared<MockMessage>();
  EXPECT_CALL(*mock_channel_, createMessage()).WillOnce(Return(msg));
  EXPECT_CALL(*mock_channel_, sendMessage(_, _, _))
      .WillOnce(
          Invoke([](AsyncCallback&& cob, auto, auto) { cob({IO_TIMEOUT}); }));
  EXPECT_CALL(*mock_channel_, recvMessage(_, _)).Times(0);

  // Act
  const auto& respTry = co_await co_awaitTry(
      client_->call<SomeObject, SomeObject>("testMethod", {}));

  // Assert
  EXPECT_TRUE(respTry.withException([](const RpcError& e) {
    EXPECT_EQ(e.code(), RpcError::Code::SEND_TIMEOUT);
  }));
}

CO_TEST_F(ClientStressTest, ThrowErrorOnRecvTimeout) {
  // Arrange
  const auto msg = std::make_shared<MockMessage>();
  EXPECT_CALL(*mock_channel_, createMessage()).WillOnce(Return(msg));
  EXPECT_CALL(*mock_channel_, sendMessage(_, _, _))
      .WillOnce(
          Invoke([](AsyncCallback&& cob, auto, auto) { cob({IOStatus::OK}); }));
  EXPECT_CALL(*mock_channel_, recvMessage(_, _))
      .WillRepeatedly(Invoke([](AsyncCallback&& cob, auto) { cob({IO_TIMEOUT}); }));

  // Act
  const auto& respTry = co_await co_awaitTry(
      client_->call<SomeObject, SomeObject>("testMethod", {}, {.recv_timeout=10ms}));

  // Assert
  EXPECT_TRUE(respTry.withException([](const RpcError& e) {
    EXPECT_EQ(e.code(), RpcError::Code::RECV_TIMEOUT);
  }));
}


CO_TEST_F(ClientStressTest, OutOfOrderDelivery) {
  // Arrange
  auto request_msg1 = std::make_shared<MockMessage>();
  auto response_msg1 = std::make_shared<MockMessage>();

  auto request_msg2 = std::make_shared<MockMessage>();
  auto response_msg2 = std::make_shared<MockMessage>();

  std::latch latch{2};

  // Expect createMessage() to be called twice, one for each call.
  EXPECT_CALL(*mock_channel_, createMessage())
      .WillOnce(Return(request_msg1))
      .WillOnce(Return(request_msg2));

  // For the first call, capture the XID and set up the response.
  uint64_t captured_xid1 = 0;
  EXPECT_CALL(*request_msg1, setSequenceId(_)).WillOnce(Invoke([&](uint64_t xid1) {
    captured_xid1 = xid1;
  }));
  EXPECT_CALL(*response_msg1, getSequenceId()).WillOnce(Invoke([&] {
    return captured_xid1;
  }));

  std::string response1_body = "response1";
  EXPECT_CALL(*response_msg1, getBody()).WillOnce(ReturnRef(response1_body));
  EXPECT_CALL(*response_msg1, getErrorCode()).WillOnce(Return(RpcError::Code::OK));

  // For the second call, capture the XID and set up the response.
  uint64_t captured_xid2 = 0;
  EXPECT_CALL(*request_msg2, setSequenceId(_)).WillOnce(Invoke([&](uint64_t xid2) {
    captured_xid2 = xid2;
  }));
  EXPECT_CALL(*response_msg2, getSequenceId()).WillOnce(Invoke([&] {
    return captured_xid2;
  }));

  std::string response2_body = "response2";
  EXPECT_CALL(*response_msg2, getBody()).WillOnce(ReturnRef(response2_body));
  EXPECT_CALL(*response_msg2, getErrorCode()).WillOnce(Return(RpcError::Code::OK));

  // When sending, immediately invoke the send callback with OK.
  EXPECT_CALL(*mock_channel_, sendMessage(_, _, _))
      .WillRepeatedly(Invoke([](AsyncCallback&& cob, auto, auto) { cob({IOStatus::OK}); }));

  // Capture the two receive callbacks.
  AsyncCallback saved_recv_callback1, saved_recv_callback2;
  EXPECT_CALL(*mock_channel_, recvMessage(_, _))
      .Times(2)
      .WillOnce(Invoke([&](AsyncCallback cob, auto) {
        saved_recv_callback1 = std::move(cob);
        latch.count_down();
      }))
      .WillOnce(Invoke([&](AsyncCallback cob, auto) {
        saved_recv_callback2 = std::move(cob);
        latch.count_down();
      }));

  // Act
  auto semi1 = client_->call<SomeObject, SomeObject>("testMethod", {})
                   .scheduleOn(folly::getGlobalCPUExecutor())
                   .start();
  auto semi2 = client_->call<SomeObject, SomeObject>("testMethod", {})
                   .scheduleOn(folly::getGlobalCPUExecutor())
                   .start();

  latch.wait();

  // Simulate out-of-order arrival:
  saved_recv_callback2({IOStatus::OK, response_msg2});
  saved_recv_callback1({IOStatus::OK, response_msg1});

  auto responses =
      co_await folly::coro::collectAll(std::move(semi1), std::move(semi2));
  auto& res1 = std::get<0>(responses);
  auto& res2 = std::get<1>(responses);

  // Assert
  EXPECT_EQ(res1.data(), "response1");
  EXPECT_EQ(res2.data(), "response2");
}

CO_TEST_F(ClientStressTest, ServerError) {
  // Arrange
  auto request_msg = std::make_shared<MockMessage>();
  auto response_msg = std::make_shared<MockMessage>();

  EXPECT_CALL(*mock_channel_, createMessage()).WillOnce(Return(request_msg));

  EXPECT_CALL(*request_msg, setMethod("testMethod"));
  EXPECT_CALL(*response_msg, getErrorCode())
      .WillOnce(Return(FAILURE));

  EXPECT_CALL(*mock_channel_, sendMessage(_, _, _))
      .WillOnce(Invoke([](AsyncCallback&& callback, auto, auto) {
        callback({IOStatus::OK});
      }));

  EXPECT_CALL(*mock_channel_, recvMessage(_, _))
      .WillOnce(Invoke([response_msg](AsyncCallback&& callback, auto) {
        callback({IOStatus::OK, response_msg});
      }));

  EXPECT_CALL(*response_msg, getSequenceId()).WillOnce(Return(0));
  std::string response_body = "some error message";
  EXPECT_CALL(*response_msg, getBody()).WillOnce(ReturnRef(response_body));

  // Act
  const auto& respTry = co_await co_awaitTry(
      client_->call<SomeObject, SomeObject>("testMethod", {}));

  // Assert
  EXPECT_TRUE(respTry.hasException());
  respTry.withException<void>([&](const RpcError& e) {
    EXPECT_EQ(e.code(), RpcError::Code::FAILURE);
    EXPECT_EQ(e.what(), response_body);
  });
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  folly::Init init(&argc, &argv);
  return RUN_ALL_TESTS();
}