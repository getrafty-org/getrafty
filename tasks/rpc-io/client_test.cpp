#include "client.hpp"
#include <folly/futures/Future.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <memory>

using namespace std::chrono;
using namespace getrafty::rpc;
using namespace getrafty::rpc::io;
using namespace std::chrono_literals;

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;

class MockMessage : public IMessage {
 public:
  MOCK_METHOD(void, writeXID, (uint64_t value), (override));
  MOCK_METHOD(void, writeInt32, (int32_t value), (override));
  MOCK_METHOD(void, writeInt64, (int64_t value), (override));
  MOCK_METHOD(void, writeBytes,
              (const std::vector<uint8_t>& bytes, uint32_t length), (override));
  MOCK_METHOD(void, writeString, (const std::string& str), (override));
  MOCK_METHOD(uint64_t, consumeXID, (), (override));
  MOCK_METHOD(int32_t, consumeInt32, (), (override));
  MOCK_METHOD(int64_t, consumeInt64, (), (override));
  MOCK_METHOD(std::vector<uint8_t>, consumeBytes, (uint32_t length),
              (override));
  MOCK_METHOD(std::string, consumeString, (), (override));
};

class MockChannel : public IAsyncChannel {
 public:
  MOCK_METHOD(MessagePtr, createMessage, (), (override));
  MOCK_METHOD(void, sendMessage, (AsyncCallback&&, MessagePtr), (override));
  MOCK_METHOD(void, recvMessage, (AsyncCallback&&), (override));
};

class SomeObject : public ISerializable {
 public:
  SomeObject() = default;

  void serialize(IMessage& m) const override { m.writeString(data_); }
  void deserialize(IMessage& m) override { data_ = m.consumeString(); }
  std::string& data() { return data_; }

 private:
  std::string data_;
};

class ClientTest : public ::testing::Test {
 protected:
  std::shared_ptr<StrictMock<MockChannel>> mock_channel_{
      std::make_shared<StrictMock<MockChannel>>()};

  std::unique_ptr<Client> client_;

  void SetUp() override {
    auto tp = std::make_shared<ThreadPool>(1);
    auto timer = std::make_shared<Timer>(EventWatcher::getInstance(), tp);
    tp->start();
    client_ = std::make_unique<Client>(mock_channel_, timer);
  }
};

TEST_F(ClientTest, JustWorks) {
  const auto request_msg = std::make_shared<MockMessage>();
  const auto response_msg = std::make_shared<MockMessage>();

  EXPECT_CALL(*mock_channel_, createMessage()).WillOnce(Return(request_msg));

  EXPECT_CALL(*request_msg, writeString("someData"));

  bool send_callback_called = false;
  EXPECT_CALL(*mock_channel_, sendMessage(_, _))
      .WillOnce(
          Invoke([&send_callback_called](const AsyncCallback& callback, auto) {
            send_callback_called = true;
            callback({OK});
          }));

  bool receive_callback_called = false;
  EXPECT_CALL(*mock_channel_, recvMessage(_))
      .WillOnce(Invoke([&receive_callback_called,
                        response_msg](const AsyncCallback& callback) {
        receive_callback_called = true;
        callback({OK, response_msg});
      }));

  EXPECT_CALL(*response_msg, consumeXID()).WillOnce(Return(0));

  EXPECT_CALL(*response_msg, consumeString()).WillOnce(Return("otherData"));

  SomeObject request;
  request.data() = "someData";
  auto future = client_->call<SomeObject, SomeObject>(request);
  auto response = std::move(future).get();

  EXPECT_TRUE(send_callback_called);
  EXPECT_TRUE(receive_callback_called);
  EXPECT_EQ(response.data(), "otherData");
};

TEST_F(ClientTest, SendTimeout) {
  const auto msg = std::make_shared<MockMessage>();
  EXPECT_CALL(*mock_channel_, createMessage()).WillOnce(Return(msg));
  EXPECT_CALL(*mock_channel_, sendMessage(_, _))
      .WillOnce(Invoke([](const AsyncCallback&, auto) {
        // Packet lost
      }));
  EXPECT_CALL(*mock_channel_, recvMessage(_)).Times(0);

  auto future =
      client_->call<SomeObject, SomeObject>({}, {.send_timeout = 10ms});

  EXPECT_THROW(
      {
        try {
          std::move(future).get(100ms);
        } catch (RpcError& e) {
          EXPECT_EQ(e.code(), RpcError::SEND_TIMEOUT);
          throw;
        }
      },
      RpcError);
}

TEST_F(ClientTest, RecvTimeout) {
  const auto msg = std::make_shared<MockMessage>();
  EXPECT_CALL(*mock_channel_, createMessage()).WillOnce(Return(msg));
  EXPECT_CALL(*mock_channel_, sendMessage(_, _))
      .WillOnce(Invoke([](const AsyncCallback& cob, auto) { cob({OK}); }));
  EXPECT_CALL(*mock_channel_, recvMessage(_))
      .WillOnce(Invoke([](const AsyncCallback& cob) {
        // Packet lost
      }));

  auto future =
      client_->call<SomeObject, SomeObject>({}, {.recv_timeout = 10ms});

  EXPECT_THROW(
      {
        try {
          std::move(future).get(100ms);
        } catch (RpcError& e) {
          EXPECT_EQ(e.code(), RpcError::RECV_TIMEOUT);
          throw;
        }
      },
      RpcError);
}

TEST_F(ClientTest, OutOfOrder) {
  auto request_msg1 = std::make_shared<MockMessage>();
  auto request_msg2 = std::make_shared<MockMessage>();
  auto response_msg1 = std::make_shared<MockMessage>();
  auto response_msg2 = std::make_shared<MockMessage>();

  EXPECT_CALL(*mock_channel_, createMessage())
      .WillOnce(Return(request_msg1))
      .WillOnce(Return(request_msg2));

  // 1. request -> response
  uint64_t captured_xid1 = 0;
  EXPECT_CALL(*request_msg1, writeXID(_))
      .WillOnce(Invoke([&](const uint64_t xid1) { captured_xid1 = xid1; }));

  EXPECT_CALL(*response_msg1, consumeXID()).WillOnce(Invoke([&] {
    return captured_xid1;
  }));

  EXPECT_CALL(*response_msg1, consumeString()).WillOnce(Return("response1"));
  //

  // 2. request -> response
  uint64_t captured_xid2 = 0;
  EXPECT_CALL(*request_msg2, writeXID(_))
      .WillOnce(Invoke([&](const uint64_t xid2) { captured_xid2 = xid2; }));

  EXPECT_CALL(*response_msg2, consumeXID()).WillOnce(Invoke([&] {
    return captured_xid2;
  }));

  EXPECT_CALL(*response_msg2, consumeString()).WillOnce(Return("response2"));
  //

  EXPECT_CALL(*mock_channel_, sendMessage(_, _))
      .WillRepeatedly(Invoke([](AsyncCallback&& cob, auto) { cob({OK}); }));

  AsyncCallback saved_recv_callback1{};
  AsyncCallback saved_recv_callback2{};
  EXPECT_CALL(*mock_channel_, recvMessage(_))
      .WillOnce(Invoke(
          [&](AsyncCallback cob) { saved_recv_callback1 = std::move(cob); }))
      .WillOnce(Invoke(
          [&](AsyncCallback cob) { saved_recv_callback2 = std::move(cob); }));

  auto future1 = client_->call<SomeObject, SomeObject>({});
  auto future2 = client_->call<SomeObject, SomeObject>({});

  // Out of order: first arrives message #2 then message #1
  saved_recv_callback2({OK, response_msg2});
  saved_recv_callback1({OK, response_msg1});

  EXPECT_NO_THROW({
    auto response1 = std::move(future1).get();
    auto response2 = std::move(future2).get();

    EXPECT_EQ(response1.data(), "response1");
    EXPECT_EQ(response2.data(), "response2");
  });
}