#include <memory>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <folly/futures/Future.h>
#include <chrono>
#include "client.hpp"

using namespace getrafty::rpc;
using namespace getrafty::rpc::io;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;

using namespace std::literals::chrono_literals;

class MockClock : public IClock {
public:
  MOCK_METHOD(std::chrono::time_point<std::chrono::milliseconds>, time, (), (const, override));
};

class MockMessage : public IAsyncChannel::IMessage {
 public:
  MOCK_METHOD(void, writeInt32, (std::int32_t value), (override));
  MOCK_METHOD(void, writeInt64, (std::int64_t value), (override));
  MOCK_METHOD(void, writeBytes,
              (const std::vector<std::uint8_t>& bytes, uint32_t length),
              (override));
  MOCK_METHOD(void, writeString, (const std::string& str), (override));
  MOCK_METHOD(std::int32_t, readInt32, (), (override));
  MOCK_METHOD(std::int64_t, readInt64, (), (override));
  MOCK_METHOD(std::vector<std::uint8_t>, readBytes, (uint32_t length),
              (override));
  MOCK_METHOD(std::string, readString, (), (override));
  MOCK_METHOD(void, setHeader,
              (const std::string& key, const std::string& value), (override));
  MOCK_METHOD((std::optional<std::string>), getHeader, (const std::string& key),
              (override));
};

class MockChannel : public IAsyncChannel {
 public:
  MOCK_METHOD(MessagePtr, createMessage, (), (override));
  MOCK_METHOD(void, sendAsync,
              (MessagePtr, std::function<void(Status)>, std::chrono::milliseconds),
              (override));
  MOCK_METHOD(void, receiveAsync,
              (std::function<void(MessagePtr, Status)>, std::chrono::milliseconds),
              (override));
};

class Object : public Serializable {
 public:
  Object()= default;
  explicit Object(std::string data) : data_(std::move(data)) {}

  void writeToMessage(
      const std::shared_ptr<IAsyncChannel::IMessage> msg) const override {
    msg->writeString(data_);
  }

  void readFromMessage(const std::shared_ptr<IAsyncChannel::IMessage> msg) override {
    data_ = msg->readString();
  }

  std::string& getData() { return data_; }

 private:
  std::string data_;
};

class ClientTest : public ::testing::Test {
 protected:
  std::shared_ptr<StrictMock<MockChannel>> mock_channel_{
      std::make_shared<StrictMock<MockChannel>>()};

  std::shared_ptr<StrictMock<MockClock>> mock_clock_{
    std::make_shared<StrictMock<MockClock>>()};


  std::unique_ptr<Client> client_;

  void SetUp() override { client_ = std::make_unique<Client>(mock_channel_, mock_clock_); }
};

TEST_F(ClientTest, JustWorks) {
  const auto request_msg = std::make_shared<MockMessage>();
  const auto response_msg = std::make_shared<MockMessage>();

  EXPECT_CALL(*mock_channel_, createMessage())
      .WillOnce(Return(request_msg));

  EXPECT_CALL(*request_msg, writeString("someData"));

  bool send_callback_called = false;
  EXPECT_CALL(*mock_channel_, sendAsync(_, _, _))
      .WillOnce(
          Invoke([&send_callback_called](auto, auto callback, auto) {
            send_callback_called = true;
            callback(IAsyncChannel::OK);
          }));

  bool receive_callback_called = false;
  EXPECT_CALL(*mock_channel_, receiveAsync(_, _))
      .WillOnce(Invoke(
          [&receive_callback_called, response_msg](auto callback, auto) {
            receive_callback_called = true;
            callback(response_msg, IAsyncChannel::OK);
          }));

  EXPECT_CALL(*response_msg, getHeader(std::string("xid")))
      .WillOnce(Return("0"));

  EXPECT_CALL(*response_msg, readString())
      .WillOnce(Return("otherData"));

  auto future = client_->call<Object, Object>(Object("someData"));
  auto response = std::move(future).get();

  EXPECT_TRUE(send_callback_called);
  EXPECT_TRUE(receive_callback_called);
  EXPECT_EQ(response.getData(), "otherData");
};

TEST_F(ClientTest, ReceiveTimeout) {
  auto request_msg = std::make_shared<MockMessage>();

  EXPECT_CALL(*mock_clock_, time())
  .WillOnce(Return(std::chrono::time_point<std::chrono::milliseconds>(10ms)))
  .WillOnce(Return(std::chrono::time_point<std::chrono::milliseconds>(50ms)));


  EXPECT_CALL(*mock_channel_, createMessage())
      .WillOnce(Return(request_msg));

  EXPECT_CALL(*request_msg, setHeader("xid", "0"));
  EXPECT_CALL(*request_msg, writeString("someData"));






  EXPECT_CALL(*mock_channel_, sendAsync(_, _, std::chrono::milliseconds(50)))
      .WillOnce(Invoke([](auto, auto callback, auto) {
        callback(IAsyncChannel::OK);
      }));

  EXPECT_CALL(*mock_channel_, receiveAsync(_, std::chrono::milliseconds(10)))
      .WillOnce(Invoke([](auto callback, auto) {
        callback(nullptr, IAsyncChannel::ERR_RECV_TIMEOUT);
      }));

  auto future = client_->call<Object, Object>(Object("timeoutData"),
                                              Client::CallOptions{50});

  // Because we simulated a timeout, the future throws RpcError
  EXPECT_THROW(std::move(future).get(), RpcError);
}


//
// TEST_F(ClientTest, OutOfOrder) {
//   auto request_msg1 = std::make_shared<MockMessage>();
//   auto request_msg2 = std::make_shared<MockMessage>();
//   auto response_msg1 = std::make_shared<MockMessage>();
//   auto response_msg2 = std::make_shared<MockMessage>();
//
//   EXPECT_CALL(*mock_channel_, createMessage())
//       .WillOnce(Return(request_msg1))
//       .WillOnce(Return(request_msg2));
//
//   EXPECT_CALL(*request_msg1, setHeader(std::string("xid"), "0"));
//   EXPECT_CALL(*request_msg1, writeString("request1"));
//
//   EXPECT_CALL(*request_msg2, setHeader(std::string("xid"), "1"));
//   EXPECT_CALL(*request_msg2, writeString("request2"));
//
//   std::function<void()> saved_send_callback1;
//   std::function<void()> saved_send_callback2;
//   EXPECT_CALL(*mock_channel_, sendAsync(_, _, _))
//       .WillOnce(Invoke([&saved_send_callback1](auto, auto callback, auto) {
//         saved_send_callback1 = callback;
//       }))
//       .WillOnce(Invoke([&saved_send_callback2](auto, auto callback, auto) {
//         saved_send_callback2 = callback;
//       }));
//
//   std::function<void(std::shared_ptr<IChannel::IMessage>)>
//       saved_receive_callback1;
//   std::function<void(std::shared_ptr<IChannel::IMessage>)>
//       saved_receive_callback2;
//   EXPECT_CALL(*mock_channel_, receiveAsync(_, _))
//       .WillOnce(Invoke([&saved_receive_callback1](auto callback, auto) {
//         saved_receive_callback1 = callback;
//       }))
//       .WillOnce(Invoke([&saved_receive_callback2](auto callback, auto) {
//         saved_receive_callback2 = callback;
//       }));
//
//   EXPECT_CALL(*response_msg1, getHeader(std::string("xid")))
//       .WillOnce(Return("0"));
//   EXPECT_CALL(*response_msg1, readString()).WillOnce(Return("response1"));
//
//   EXPECT_CALL(*response_msg2, getHeader(std::string("xid")))
//       .WillOnce(Return("1"));
//   EXPECT_CALL(*response_msg2, readString()).WillOnce(Return("response2"));
//
//   auto future1 = client_->call<AnyDto, AnyDto>(AnyDto("request1"));
//   auto future2 = client_->call<AnyDto, AnyDto>(AnyDto("request2"));
//
//   saved_send_callback1();
//   saved_send_callback2();
//
//   // Deliver responses in reverse order: 2 then 1
//   saved_receive_callback2(response_msg2);
//   saved_receive_callback1(response_msg1);
//
//   auto response2 = std::move(future2).get();
//   auto response1 = std::move(future1).get();
//
//   EXPECT_EQ(response1.getData(), "response1");
//   EXPECT_EQ(response2.getData(), "response2");
// }