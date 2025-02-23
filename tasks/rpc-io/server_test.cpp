// server_test.cpp

#include "server.hpp"
#include <folly/coro/BlockingWait.h>
#include <folly/experimental/coro/GtestHelpers.h>
#include <folly/init/Init.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <latch>
#include <memory>

#include "client.hpp"

using namespace std::chrono_literals;
using namespace getrafty::rpc;
using namespace getrafty::rpc::io;
using ::testing::_;

class SomeObject : public ISerializable {
 public:
  SomeObject() = default;
  void serialize(IMessage& m) const override { ; }
  void deserialize(IMessage& m) override { ; }
  std::string& data() { return data_; }
  [[nodiscard]] const std::string& data() const { return data_; }
 private:
  std::string data_;
};

class ServerTest : public ::testing::Test {
  void SetUp() override {
  }
};

CO_TEST_F(ServerTest, EchoWorks) {
  Server server {"mem://?id=12345"};
  server.addHandler<SomeObject, SomeObject>(
      "echo",
      [](const SomeObject &req) -> folly::coro::Task<SomeObject> {
        SomeObject resp;
        resp.data() = req.data();
        co_return resp;
      }
  );

  // tcp://X.X.X.X:0000?cluster=host1,host2,host3
  // file://home/user/file.q?syncOnWrite=0
  Client client {"mem://?id=12345"};
  co_await client.start();

  SomeObject request{};
  request.data() = "1";
  auto response = co_await client.call<SomeObject, SomeObject>("echo", {});
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  folly::Init init(&argc, &argv);
  return RUN_ALL_TESTS();
}
